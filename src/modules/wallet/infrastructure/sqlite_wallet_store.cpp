#include "modules/wallet/infrastructure/sqlite_wallet_store.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <new>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sqlite3.h>

namespace fiaisdk::wallet {
namespace {

constexpr int kKeyPairSize = 64;
constexpr int kNonceSize = 12;
constexpr int kTagSize = 16;
using SealedKeys = std::array<std::uint8_t, kNonceSize + kKeyPairSize + kTagSize>;
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

core::Error sqliteError(int code) {
    if (code == SQLITE_NOMEM) {
        throw std::bad_alloc();
    }
    if (code == SQLITE_BUSY || code == SQLITE_LOCKED || code == SQLITE_CANTOPEN) {
        return core::Error::unavailable("wallet database unavailable");
    }
    return core::Error::internal("wallet database operation failed");
}

core::Result<Statement> prepare(sqlite3 *db, const char *sql) {
    sqlite3_stmt *raw = nullptr;
    const int code = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    Statement statement(raw, sqlite3_finalize);
    if (code != SQLITE_OK) {
        return sqliteError(code);
    }
    return statement;
}

template <std::size_t N> bool readBlob(sqlite3_stmt *stmt, std::array<std::uint8_t, N> &output) {
    if (sqlite3_column_type(stmt, 0) != SQLITE_BLOB ||
        sqlite3_column_bytes(stmt, 0) != static_cast<int>(N)) {
        return false;
    }
    const auto *blob = static_cast<const std::uint8_t *>(sqlite3_column_blob(stmt, 0));
    if (blob == nullptr) {
        return false;
    }
    std::ranges::copy(std::span(blob, N), output.begin());
    return true;
}

// Address bytes authenticate each wallet's identity. The database key verifier uses empty AAD.
core::Result<void> crypt(bool encrypt, std::span<const std::uint8_t> key,
                         std::span<const std::uint8_t> aad, SealedKeys &sealed,
                         std::span<std::uint8_t> plain) {
    auto bytes = std::span(sealed);
    auto nonce = bytes.first<kNonceSize>();
    auto payload = bytes.subspan<kNonceSize, kKeyPairSize>();
    auto tag = bytes.last<kTagSize>();
    const auto fail = core::Error::internal("wallet key encryption operation failed");
    const auto freeContext = [](OSSL_LIB_CTX *context) {
        OPENSSL_thread_stop_ex(context);
        OSSL_LIB_CTX_free(context);
    };
    std::unique_ptr<OSSL_LIB_CTX, decltype(freeContext)> context(OSSL_LIB_CTX_new(), freeContext);
    if (!context || (encrypt && RAND_bytes_ex(context.get(), nonce.data(), nonce.size(), 0) != 1)) {
        return fail;
    }
    std::unique_ptr<EVP_CIPHER, decltype(&EVP_CIPHER_free)> cipher(
        EVP_CIPHER_fetch(context.get(), "AES-256-GCM", nullptr), EVP_CIPHER_free);
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),
                                                                        EVP_CIPHER_CTX_free);
    int written = 0;
    if (!cipher || !ctx ||
        EVP_CipherInit_ex(ctx.get(), cipher.get(), nullptr, key.data(), nonce.data(),
                          encrypt ? 1 : 0) != 1 ||
        (!aad.empty() && EVP_CipherUpdate(ctx.get(), nullptr, &written, aad.data(),
                                          static_cast<int>(aad.size())) != 1)) {
        return fail;
    }
    auto *output = encrypt ? payload.data() : plain.data();
    const auto *input = encrypt ? plain.data() : payload.data();
    if (EVP_CipherUpdate(ctx.get(), output, &written, input, kKeyPairSize) != 1 ||
        written != kKeyPairSize ||
        (!encrypt &&
         EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, kTagSize, tag.data()) != 1)) {
        return fail;
    }
    std::array<std::uint8_t, EVP_MAX_BLOCK_LENGTH> final{};
    const int verified = EVP_CipherFinal_ex(ctx.get(), final.data(), &written);
    core::secureZero(final);
    if (verified != 1) {
        return encrypt ? fail : core::Error::invalidArgument("wallet key authentication failed");
    }
    if (written != 0 || (encrypt && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, kTagSize,
                                                        tag.data()) != 1)) {
        return fail;
    }
    return {};
}

core::Result<void> execute(sqlite3 *db, const char *sql) {
    const int code = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    if (code != SQLITE_OK) {
        return sqliteError(code);
    }
    return {};
}

core::Result<void> initialize(sqlite3 *db, std::span<const std::uint8_t, 32> key) {
    // The caller closes the connection on failure, rolling back any unfinished transaction.
    if (auto begun = execute(db, "BEGIN IMMEDIATE"); !begun) {
        return begun.error();
    }
    int version = 0;
    {
        auto stmt = prepare(db, "PRAGMA user_version");
        if (!stmt) {
            return stmt.error();
        }
        const int code = sqlite3_step(stmt.value().get());
        if (code != SQLITE_ROW) {
            return sqliteError(code);
        }
        version = sqlite3_column_int(stmt.value().get(), 0);
    }
    core::SecureBytes check(kKeyPairSize);
    SealedKeys verifier{};
    if (version == 0) {
        auto tables = prepare(db, "SELECT 1 FROM sqlite_schema LIMIT 1");
        if (!tables) {
            return tables.error();
        }
        const int code = sqlite3_step(tables.value().get());
        if (code == SQLITE_ROW) {
            return core::Error::invalidArgument("wallet database path must be dedicated");
        }
        if (code != SQLITE_DONE) {
            return sqliteError(code);
        }
        if (auto schema = execute(
                db, "CREATE TABLE wallet_meta (id INTEGER PRIMARY KEY CHECK(id=1),"
                    " verifier BLOB NOT NULL CHECK(length(verifier)=92)) STRICT;"
                    "CREATE TABLE wallets (address BLOB PRIMARY KEY CHECK(length(address)=20),"
                    " sealed BLOB NOT NULL CHECK(length(sealed)=92)) STRICT, WITHOUT ROWID;"
                    "PRAGMA user_version=1");
            !schema) {
            return schema.error();
        }
        if (auto encrypted = crypt(true, key, {}, verifier, check.mutableView()); !encrypted) {
            return encrypted.error();
        }
        auto stmt = prepare(db, "INSERT INTO wallet_meta VALUES(1, ?1)");
        if (!stmt) {
            return stmt.error();
        }
        const int bind = sqlite3_bind_blob(stmt.value().get(), 1, verifier.data(),
                                           static_cast<int>(verifier.size()), SQLITE_TRANSIENT);
        if (bind != SQLITE_OK) {
            return sqliteError(bind);
        }
        const int inserted = sqlite3_step(stmt.value().get());
        if (inserted != SQLITE_DONE) {
            return sqliteError(inserted);
        }
    } else if (version == 1) {
        auto stmt = prepare(db, "SELECT verifier FROM wallet_meta WHERE id=1");
        if (!stmt) {
            return stmt.error();
        }
        const int code = sqlite3_step(stmt.value().get());
        if (code != SQLITE_ROW) {
            return sqliteError(code);
        }
        if (!readBlob(stmt.value().get(), verifier)) {
            return core::Error::internal("invalid wallet database verifier");
        }
        if (auto verified = crypt(false, key, {}, verifier, check.mutableView()); !verified) {
            return verified.error();
        }
    } else {
        return core::Error::invalidArgument("unsupported wallet database schema version");
    }
    return execute(db, "COMMIT");
}

core::Result<Statement> withAddress(sqlite3 *db, const char *sql, const WalletAddress &address) {
    auto stmt = prepare(db, sql);
    if (!stmt) {
        return stmt.error();
    }
    const int code = sqlite3_bind_blob(stmt.value().get(), 1, address.data(),
                                       static_cast<int>(address.size()), SQLITE_TRANSIENT);
    if (code != SQLITE_OK) {
        return sqliteError(code);
    }
    return stmt;
}

class SqliteWalletStore final : public IWalletStore {
  public:
    SqliteWalletStore(Database db, std::span<const std::uint8_t, 32> key)
        : db_(std::move(db)), key_(key) {}

    core::Result<void> save(const Wallet &wallet) override {
        std::lock_guard lock(mutex_);
        auto stmt = withAddress(
            db_.get(),
            "INSERT INTO wallets(address, sealed) VALUES(?1, ?2) ON CONFLICT(address) DO NOTHING",
            wallet.info().address);
        if (!stmt) {
            return stmt.error();
        }
        core::SecureBytes plain(kKeyPairSize);
        std::ranges::copy(wallet.blsSecret(), plain.mutableView().first<32>().begin());
        std::ranges::copy(wallet.secpSecret(), plain.mutableView().last<32>().begin());
        SealedKeys sealed{};
        if (auto encrypted =
                crypt(true, key_.view(), wallet.info().address, sealed, plain.mutableView());
            !encrypted) {
            return encrypted.error();
        }
        const int bound = sqlite3_bind_blob(stmt.value().get(), 2, sealed.data(),
                                            static_cast<int>(sealed.size()), SQLITE_TRANSIENT);
        if (bound != SQLITE_OK) {
            return sqliteError(bound);
        }
        const int code = sqlite3_step(stmt.value().get());
        if (code != SQLITE_DONE) {
            return sqliteError(code);
        }
        if (sqlite3_changes(db_.get()) == 0) {
            return core::Error{core::ErrorCode::AlreadyExists, "wallet already exists"};
        }
        return {};
    }

    core::Result<core::SecureBytes> load(const WalletAddress &address) override {
        std::lock_guard lock(mutex_);
        auto stmt = withAddress(db_.get(), "SELECT sealed FROM wallets WHERE address=?1", address);
        if (!stmt) {
            return stmt.error();
        }
        const int code = sqlite3_step(stmt.value().get());
        if (code == SQLITE_DONE) {
            return core::Error{core::ErrorCode::NotFound, "wallet not found"};
        }
        if (code != SQLITE_ROW) {
            return sqliteError(code);
        }
        SealedKeys sealed{};
        core::SecureBytes plain(kKeyPairSize);
        if (!readBlob(stmt.value().get(), sealed) ||
            !crypt(false, key_.view(), address, sealed, plain.mutableView())) {
            return core::Error::internal("stored wallet keys failed authentication");
        }
        return plain;
    }

    core::Result<std::vector<WalletAddress>> addresses() override {
        std::lock_guard lock(mutex_);
        auto stmt = prepare(db_.get(), "SELECT address FROM wallets ORDER BY address");
        if (!stmt) {
            return stmt.error();
        }
        std::vector<WalletAddress> output;
        for (int code = sqlite3_step(stmt.value().get()); code != SQLITE_DONE;
             code = sqlite3_step(stmt.value().get())) {
            if (code != SQLITE_ROW) {
                return sqliteError(code);
            }
            WalletAddress address{};
            if (!readBlob(stmt.value().get(), address)) {
                return core::Error::internal("invalid stored wallet address");
            }
            output.push_back(address);
        }
        return output;
    }

    core::Result<void> remove(const WalletAddress &address) override {
        std::lock_guard lock(mutex_);
        auto stmt = withAddress(db_.get(), "DELETE FROM wallets WHERE address=?1", address);
        if (!stmt) {
            return stmt.error();
        }
        const int code = sqlite3_step(stmt.value().get());
        if (code != SQLITE_DONE) {
            return sqliteError(code);
        }
        if (sqlite3_changes(db_.get()) == 0) {
            return core::Error{core::ErrorCode::NotFound, "wallet not found"};
        }
        return {};
    }

  private:
    Database db_;
    core::SecureBytes key_;
    std::mutex mutex_;
};

} // namespace

core::Result<std::unique_ptr<IWalletStore>>
openSqliteWalletStore(const char *path, std::span<const std::uint8_t, 32> key) {
    sqlite3 *raw = nullptr;
    const int opened = sqlite3_open_v2(
        path, &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    Database db(raw, sqlite3_close);
    if (opened != SQLITE_OK) {
        return sqliteError(opened);
    }
    if (sqlite3_busy_timeout(db.get(), 2000) != SQLITE_OK ||
        sqlite3_db_config(db.get(), SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr) != SQLITE_OK ||
        sqlite3_db_config(db.get(), SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr) != SQLITE_OK ||
        sqlite3_db_config(db.get(), SQLITE_DBCONFIG_ENABLE_TRIGGER, 0, nullptr) != SQLITE_OK ||
        sqlite3_db_config(db.get(), SQLITE_DBCONFIG_ENABLE_VIEW, 0, nullptr) != SQLITE_OK) {
        return core::Error::internal("could not configure wallet database");
    }
    if (auto initialized = initialize(db.get(), key); !initialized) {
        return initialized.error();
    }
    std::unique_ptr<IWalletStore> store = std::make_unique<SqliteWalletStore>(std::move(db), key);
    return store;
}

} // namespace fiaisdk::wallet
