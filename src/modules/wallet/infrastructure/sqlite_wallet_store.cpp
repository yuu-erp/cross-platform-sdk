#include "modules/wallet/infrastructure/sqlite_wallet_store.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <new>
#include <sqlite3.h>
#include <utility>

namespace fiaisdk::wallet {
namespace {

constexpr std::size_t kKeyPairSize = 64;
// Stored layout is crypto's AEAD layout: nonce (12) || ciphertext (64) || tag (16).
using SealedKeys = std::array<std::uint8_t, kKeyPairSize + crypto::kAeadOverhead>;
static_assert(std::tuple_size_v<SealedKeys> == 92, "schema v1 stores 92-byte sealed blobs");
using StoreKey = std::span<const std::uint8_t, crypto::kAeadKeySize>;
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
core::Result<SealedKeys> seal(const crypto::CryptoFacade &crypto, StoreKey key,
                              std::span<const std::uint8_t> aad,
                              std::span<const std::uint8_t> plain) {
    auto sealed = crypto.aeadSeal(key, aad, plain);
    if (!sealed) {
        return sealed.error();
    }
    SealedKeys output{};
    if (sealed.value().size() != output.size()) {
        return core::Error::internal("unexpected sealed wallet size");
    }
    std::ranges::copy(sealed.value(), output.begin());
    return output;
}

core::Result<core::SecureBytes> open(const crypto::CryptoFacade &crypto, StoreKey key,
                                     std::span<const std::uint8_t> aad, const SealedKeys &sealed) {
    auto plain = crypto.aeadOpen(key, aad, sealed);
    if (plain && plain.value().size() != kKeyPairSize) {
        return core::Error::internal("unexpected opened wallet size");
    }
    return plain;
}

core::Result<void> execute(sqlite3 *db, const char *sql) {
    const int code = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    if (code != SQLITE_OK) {
        return sqliteError(code);
    }
    return {};
}

core::Result<void> initialize(sqlite3 *db, const crypto::CryptoFacade &crypto, StoreKey key) {
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
    const core::SecureBytes check(kKeyPairSize);
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
        auto verifier = seal(crypto, key, {}, check.view());
        if (!verifier) {
            return verifier.error();
        }
        auto stmt = prepare(db, "INSERT INTO wallet_meta VALUES(1, ?1)");
        if (!stmt) {
            return stmt.error();
        }
        const int bind =
            sqlite3_bind_blob(stmt.value().get(), 1, verifier.value().data(),
                              static_cast<int>(verifier.value().size()), SQLITE_TRANSIENT);
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
        SealedKeys verifier{};
        if (!readBlob(stmt.value().get(), verifier)) {
            return core::Error::internal("invalid wallet database verifier");
        }
        auto verified = open(crypto, key, {}, verifier);
        if (!verified) {
            if (verified.error().code == core::ErrorCode::AuthenticationFailed) {
                return core::Error{core::ErrorCode::AuthenticationFailed,
                                   "wallet database key rejected"};
            }
            return verified.error();
        }
        if (!core::constantTimeEquals(verified.value().view(), check.view())) {
            return core::Error::internal("invalid wallet database verifier");
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
    SqliteWalletStore(Database db, StoreKey key, std::shared_ptr<const crypto::CryptoFacade> crypto)
        : db_(std::move(db)), key_(key), crypto_(std::move(crypto)) {}

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
        auto sealed = seal(*crypto_, storeKey(), wallet.info().address, plain.view());
        if (!sealed) {
            return sealed.error();
        }
        const int bound =
            sqlite3_bind_blob(stmt.value().get(), 2, sealed.value().data(),
                              static_cast<int>(sealed.value().size()), SQLITE_TRANSIENT);
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
        if (!readBlob(stmt.value().get(), sealed)) {
            return core::Error::internal("invalid stored wallet record");
        }
        auto plain = open(*crypto_, storeKey(), address, sealed);
        if (!plain) {
            // The store key was verified at open, so a tag failure means a tampered record.
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
    [[nodiscard]] StoreKey storeKey() const {
        return key_.view().first<crypto::kAeadKeySize>();
    }

    Database db_;
    core::SecureBytes key_;
    std::shared_ptr<const crypto::CryptoFacade> crypto_;
    std::mutex mutex_;
};

} // namespace

core::Result<std::unique_ptr<IWalletStore>>
openSqliteWalletStore(const char *path, std::span<const std::uint8_t, 32> key,
                      std::shared_ptr<const crypto::CryptoFacade> crypto) {
    if (!crypto) {
        return core::Error::invalidArgument("wallet store requires a crypto facade");
    }
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
    // Overwrite deleted sealed records instead of leaving them in free pages.
    if (auto secure = execute(db.get(), "PRAGMA secure_delete=ON"); !secure) {
        return secure.error();
    }
    if (auto initialized = initialize(db.get(), *crypto, key); !initialized) {
        return initialized.error();
    }
    std::unique_ptr<IWalletStore> store =
        std::make_unique<SqliteWalletStore>(std::move(db), key, std::move(crypto));
    return store;
}

} // namespace fiaisdk::wallet
