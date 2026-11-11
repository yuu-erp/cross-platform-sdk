#include <fiaisdk/cpp/crypto.hpp>
#include <fiaisdk/cpp/wallet.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <sqlite3.h>
#include <thread>
#include <vector>

namespace {

void runSql(const std::string &path, const char *sql) {
    sqlite3 *raw = nullptr;
    const int opened =
        sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    ASSERT_EQ(opened, SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(db.get(), sql, nullptr, nullptr, nullptr), SQLITE_OK);
}

fiaisdk_status openStatus(const std::string &path, const fiaisdk::WalletStore::Key &key) {
    fiaisdk_wallet_store *raw = nullptr;
    const auto status = fiaisdk_wallet_store_open(path.c_str(), key.data(), key.size(), &raw);
    fiaisdk_wallet_store_close(raw);
    return status;
}

class WalletStoreTest : public testing::Test {
  protected:
    void SetUp() override {
        fiaisdk::Crypto(sdk).randomBytes(key.data(), key.size());
        std::array<std::uint8_t, 12> suffix{};
        fiaisdk::Crypto(sdk).randomBytes(suffix.data(), suffix.size());
        std::string name = "fiaisdk-wallet-";
        constexpr char digits[] = "0123456789abcdef";
        for (auto byte : suffix) {
            name += digits[byte >> 4];
            name += digits[byte & 15];
        }
        const auto candidate = std::filesystem::temp_directory_path() / name;
        ASSERT_TRUE(std::filesystem::create_directory(candidate));
        directory = candidate;
        path = (directory / "wallet.sqlite3").u8string();
    }
    void TearDown() override {
        if (!directory.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(directory, ec);
            EXPECT_FALSE(ec);
        }
    }

    fiaisdk::Sdk sdk;
    fiaisdk::WalletStore::Key key{};
    std::filesystem::path directory;
    std::string path;
};

TEST_F(WalletStoreTest, PersistsDualKeyWalletAcrossSdkAndStoreLifetimes) {
    fiaisdk::Wallet::BlsSignature bls{};
    fiaisdk::Wallet::Secp256k1Signature secp{};
    fiaisdk::WalletStore::Address address{};
    std::array<std::uint8_t, 32> digest{};
    {
        const fiaisdk::Sdk original;
        const fiaisdk::Wallet wallet(original);
        const fiaisdk::WalletStore store(path.c_str(), key);
        EXPECT_TRUE(store.addresses().empty());
        store.save(wallet);
        bls = wallet.signBls("persisted");
        secp = wallet.signDigest(digest);
        const auto info = wallet.info();
        std::copy_n(info.address, sizeof(info.address), address.bytes);
    }

    fiaisdk_wallet_store *raw = nullptr;
    auto wrongKey = key;
    wrongKey.front() ^= 1;
    EXPECT_EQ(fiaisdk_wallet_store_open(path.c_str(), wrongKey.data(), wrongKey.size(), &raw),
              FIAISDK_STATUS_AUTHENTICATION_FAILED);
    ASSERT_EQ(raw, nullptr);
    ASSERT_EQ(fiaisdk_wallet_store_open(path.c_str(), key.data(), key.size(), &raw),
              FIAISDK_STATUS_OK);
    const std::unique_ptr<fiaisdk_wallet_store, decltype(&fiaisdk_wallet_store_close)> abiStore(
        raw, fiaisdk_wallet_store_close);

    fiaisdk_wallet_address unchanged{};
    std::fill_n(unchanged.bytes, sizeof(unchanged.bytes), 0xA5);
    std::size_t capacity = 0;
    EXPECT_EQ(fiaisdk_wallet_store_list(raw, &unchanged, &capacity),
              FIAISDK_STATUS_BUFFER_TOO_SMALL);
    EXPECT_EQ(capacity, 1U);
    EXPECT_EQ(unchanged.bytes[0], 0xA5);

    const fiaisdk::WalletStore store(path.c_str(), key);
    const auto addresses = store.addresses();
    ASSERT_EQ(addresses.size(), 1U);
    EXPECT_TRUE(std::equal(std::begin(address.bytes), std::end(address.bytes),
                           std::begin(addresses.front().bytes)));
    const auto loaded = store.load(sdk, address);
    EXPECT_EQ(loaded.signBls("persisted"), bls);
    EXPECT_EQ(loaded.signDigest(digest), secp);
    EXPECT_EQ(fiaisdk_wallet_store_save(raw, loaded.native()), FIAISDK_STATUS_ALREADY_EXISTS);
    store.remove(address);
    EXPECT_EQ(fiaisdk_wallet_store_remove(raw, &address), FIAISDK_STATUS_NOT_FOUND);
    EXPECT_TRUE(store.addresses().empty());
    fiaisdk_wallet *missing = nullptr;
    EXPECT_EQ(fiaisdk_wallet_store_load(raw, sdk.native(), &address, &missing),
              FIAISDK_STATUS_NOT_FOUND);
    EXPECT_EQ(missing, nullptr);
    EXPECT_EQ(loaded.signBls("persisted"), bls);
}

TEST_F(WalletStoreTest, RejectsCiphertextMovedToAnotherAddress) {
    const fiaisdk::Wallet first(sdk);
    const fiaisdk::Wallet second(sdk);
    {
        const fiaisdk::WalletStore store(path.c_str(), key);
        store.save(first);
        store.save(second);
    }
    sqlite3 *rawDb = nullptr;
    const int opened = sqlite3_open_v2(path.c_str(), &rawDb, SQLITE_OPEN_READWRITE, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, sqlite3_close);
    ASSERT_EQ(opened, SQLITE_OK);
    ASSERT_EQ(
        sqlite3_exec(db.get(),
                     "UPDATE wallets SET sealed=(SELECT sealed FROM wallets "
                     "ORDER BY address LIMIT 1) "
                     "WHERE address=(SELECT address FROM wallets ORDER BY address DESC LIMIT 1)",
                     nullptr, nullptr, nullptr),
        SQLITE_OK);

    fiaisdk_wallet_store *rawStore = nullptr;
    ASSERT_EQ(fiaisdk_wallet_store_open(path.c_str(), key.data(), key.size(), &rawStore),
              FIAISDK_STATUS_OK);
    const std::unique_ptr<fiaisdk_wallet_store, decltype(&fiaisdk_wallet_store_close)> store(
        rawStore, fiaisdk_wallet_store_close);
    std::array<fiaisdk_wallet_address, 2> addresses{};
    std::size_t count = addresses.size();
    ASSERT_EQ(fiaisdk_wallet_store_list(store.get(), addresses.data(), &count), FIAISDK_STATUS_OK);
    ASSERT_EQ(count, addresses.size());
    fiaisdk_wallet *loaded = nullptr;
    EXPECT_EQ(fiaisdk_wallet_store_load(store.get(), sdk.native(), &addresses.back(), &loaded),
              FIAISDK_STATUS_INTERNAL_ERROR);
    EXPECT_EQ(loaded, nullptr);
    fiaisdk_wallet_destroy(loaded);
}

TEST_F(WalletStoreTest, RejectsUnsupportedSchemaAndForeignDatabases) {
    { const fiaisdk::WalletStore store(path.c_str(), key); }
    runSql(path, "PRAGMA user_version=2");
    EXPECT_EQ(openStatus(path, key), FIAISDK_STATUS_INVALID_ARGUMENT);

    const auto foreign = (directory / "foreign.sqlite3").u8string();
    runSql(foreign, "CREATE TABLE notes(id INTEGER PRIMARY KEY)");
    EXPECT_EQ(openStatus(foreign, key), FIAISDK_STATUS_INVALID_ARGUMENT);
}

TEST_F(WalletStoreTest, RemoveOverwritesTheSealedRecord) {
    const fiaisdk::Wallet wallet(sdk);
    std::vector<std::uint8_t> sealed;
    {
        const fiaisdk::WalletStore store(path.c_str(), key);
        store.save(wallet);
    }
    {
        sqlite3 *raw = nullptr;
        ASSERT_EQ(sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr), SQLITE_OK);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
        sqlite3_stmt *stmtRaw = nullptr;
        ASSERT_EQ(sqlite3_prepare_v2(db.get(), "SELECT sealed FROM wallets", -1, &stmtRaw, nullptr),
                  SQLITE_OK);
        const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(stmtRaw,
                                                                              sqlite3_finalize);
        ASSERT_EQ(sqlite3_step(stmt.get()), SQLITE_ROW);
        const auto *blob = static_cast<const std::uint8_t *>(sqlite3_column_blob(stmt.get(), 0));
        sealed.assign(blob, blob + sqlite3_column_bytes(stmt.get(), 0));
    }
    ASSERT_EQ(sealed.size(), 92U);
    const auto fileContains = [&] {
        std::ifstream file(path, std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                              std::istreambuf_iterator<char>());
        return std::search(bytes.begin(), bytes.end(), sealed.begin(), sealed.end()) != bytes.end();
    };
    ASSERT_TRUE(fileContains());
    {
        const fiaisdk::WalletStore store(path.c_str(), key);
        const auto info = wallet.info();
        fiaisdk::WalletStore::Address address{};
        std::copy_n(info.address, sizeof(info.address), address.bytes);
        store.remove(address);
    }
    EXPECT_FALSE(fileContains());
}

TEST_F(WalletStoreTest, OneStoreIsSafeToUseFromSeveralThreads) {
    const fiaisdk::WalletStore store(path.c_str(), key);
    constexpr int kThreads = 4;
    constexpr int kPerThread = 3;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kPerThread; ++i) {
                const fiaisdk::Wallet wallet(sdk);
                store.save(wallet);
                const auto info = wallet.info();
                fiaisdk::WalletStore::Address address{};
                std::copy_n(info.address, sizeof(info.address), address.bytes);
                const auto loaded = store.load(sdk, address);
                const auto loadedInfo = loaded.info();
                EXPECT_TRUE(std::equal(std::begin(loadedInfo.address), std::end(loadedInfo.address),
                                       std::begin(info.address)));
                (void)store.addresses();
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(store.addresses().size(), static_cast<std::size_t>(kThreads * kPerThread));
}

} // namespace
