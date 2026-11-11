#include "wallet_panel.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLibrary>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <array>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
template <std::size_t Size> QByteArray hex(const uint8_t (&bytes)[Size]) {
    return QByteArray(reinterpret_cast<const char *>(bytes), Size).toHex();
}

QByteArray readKey(const QLineEdit *field) {
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9A-Fa-f]{64}\\z"));
    const auto input = field->text();
    return pattern.match(input).hasMatch() ? QByteArray::fromHex(input.toLatin1()) : QByteArray{};
}

fiaisdk_wallet_address selectedAddress(const QComboBox *addresses) {
    fiaisdk_wallet_address address{};
    const auto bytes = addresses->currentData().toByteArray();
    std::memcpy(address.bytes, bytes.constData(), sizeof(address.bytes));
    return address;
}
} // namespace

WalletPanel::WalletPanel(Log log, LogResult logResult)
    : log_(std::move(log)), logResult_(std::move(logResult)) {
    auto *layout = new QVBoxLayout(this);
    const auto addButton = [this](QHBoxLayout *row, const QString &label, auto action) {
        auto *button = new QPushButton(label);
        row->addWidget(button);
        connect(button, &QPushButton::clicked, this, action);
        return button;
    };
    const auto secretField = [] {
        auto *field = new QLineEdit;
        field->setEchoMode(QLineEdit::Password);
        field->setPlaceholderText("64 hex characters");
        return field;
    };

    auto *walletForm = new QFormLayout;
    layout->addLayout(walletForm);
    blsKey_ = secretField();
    secpKey_ = secretField();
    walletForm->addRow("BLS private key", blsKey_);
    walletForm->addRow("secp256k1 private key", secpKey_);

    auto *walletActions = new QHBoxLayout;
    layout->addLayout(walletActions);
    create_ = addButton(walletActions, "Create wallet", [this] {
        fiaisdk_wallet *wallet = nullptr;
        const auto status = api_.create(sdk_, &wallet);
        logResult_("fiaisdk_wallet_create", status);
        if (status == FIAISDK_STATUS_OK) {
            replaceWallet(wallet);
            readInfo();
        }
    });
    import_ = addButton(walletActions, "Import wallet", [this] {
        auto bls = readKey(blsKey_);
        auto secp = readKey(secpKey_);
        blsKey_->clear();
        secpKey_->clear();
        if (bls.isEmpty() || secp.isEmpty()) {
            log_("Enter exactly 64 hex characters for each private key.");
        } else {
            fiaisdk_wallet *wallet = nullptr;
            const auto status = api_.import(
                sdk_, reinterpret_cast<const uint8_t *>(bls.constData()), bls.size(),
                reinterpret_cast<const uint8_t *>(secp.constData()), secp.size(), &wallet);
            logResult_("fiaisdk_wallet_import", status);
            if (status == FIAISDK_STATUS_OK) {
                replaceWallet(wallet);
                readInfo();
            }
        }
        bls.fill('\0');
        secp.fill('\0');
    });
    info_ = addButton(walletActions, "Get wallet info", [this] { readInfo(); });
    destroy_ = addButton(walletActions, "Destroy wallet", [this] {
        replaceWallet(nullptr);
        log_("fiaisdk_wallet_destroy completed");
    });

    auto *signing = new QHBoxLayout;
    layout->addLayout(signing);
    message_ = new QLineEdit("Qt wallet test");
    message_->setPlaceholderText("UTF-8 message");
    signing->addWidget(message_, 1);
    scheme_ = new QComboBox;
    scheme_->addItem("BLS12-381", FIAISDK_CRYPTO_SCHEME_BLS12381);
    scheme_->addItem("secp256k1 (Keccak-256)", FIAISDK_CRYPTO_SCHEME_SECP256K1);
    signing->addWidget(scheme_);
    sign_ = addButton(signing, "Sign and verify", [this] { signAndVerify(); });

    auto *storeForm = new QFormLayout;
    layout->addLayout(storeForm);
    auto *pathRow = new QHBoxLayout;
    path_ = new QLineEdit;
    path_->setPlaceholderText("Path to a dedicated SQLite file");
    pathRow->addWidget(path_);
    browse_ = addButton(pathRow, "Browse database", [this] {
        const auto file = QFileDialog::getSaveFileName(
            this, "Select a wallet database", path_->text(),
            "SQLite (*.sqlite3 *.db);;All files (*)", nullptr, QFileDialog::DontConfirmOverwrite);
        if (!file.isEmpty()) {
            path_->setText(file);
        }
    });
    storeForm->addRow("Database", pathRow);

    auto *keyRow = new QHBoxLayout;
    databaseKey_ = secretField();
    keyRow->addWidget(databaseKey_);
    generateKey_ = addButton(keyRow, "Generate database key", [this] {
        uint8_t key[32]{};
        const auto status = api_.random(sdk_, key, sizeof(key));
        logResult_("fiaisdk_crypto_random_bytes", status);
        if (status == FIAISDK_STATUS_OK) {
            auto encoded = hex(key);
            databaseKey_->setText(QString::fromLatin1(encoded));
            encoded.fill('\0');
        }
        std::memset(key, 0, sizeof(key));
    });
    storeForm->addRow("Database key", keyRow);

    auto *storeActions = new QHBoxLayout;
    layout->addLayout(storeActions);
    open_ = addButton(storeActions, "Open database", [this] { openStore(); });
    close_ = addButton(storeActions, "Close database", [this] { closeStore(); });
    save_ = addButton(storeActions, "Save wallet", [this] {
        const auto status = api_.save(store_, wallet_);
        logResult_("fiaisdk_wallet_store_save", status);
        if (status == FIAISDK_STATUS_OK) {
            listWallets();
        }
    });
    list_ = addButton(storeActions, "List wallets", [this] { listWallets(); });

    auto *selection = new QHBoxLayout;
    layout->addLayout(selection);
    addresses_ = new QComboBox;
    addresses_->setPlaceholderText("Stored wallet address");
    selection->addWidget(addresses_, 1);
    load_ = addButton(selection, "Load selected", [this] { loadWallet(); });
    remove_ = addButton(selection, "Remove selected", [this] { removeWallet(); });
    connect(addresses_, &QComboBox::currentIndexChanged, this, [this] { updateButtons(); });
    layout->addStretch();
    updateButtons();
}

QString WalletPanel::resolve(QLibrary &library) {
    Api resolved;
    const auto bind = [&library](auto &function, const char *name) {
        function = reinterpret_cast<std::decay_t<decltype(function)>>(library.resolve(name));
        return function != nullptr;
    };
    if (!bind(resolved.create, "fiaisdk_wallet_create") ||
        !bind(resolved.import, "fiaisdk_wallet_import") ||
        !bind(resolved.info, "fiaisdk_wallet_get_info") ||
        !bind(resolved.sign, "fiaisdk_wallet_sign") ||
        !bind(resolved.destroy, "fiaisdk_wallet_destroy") ||
        !bind(resolved.open, "fiaisdk_wallet_store_open") ||
        !bind(resolved.close, "fiaisdk_wallet_store_close") ||
        !bind(resolved.save, "fiaisdk_wallet_store_save") ||
        !bind(resolved.load, "fiaisdk_wallet_store_load") ||
        !bind(resolved.list, "fiaisdk_wallet_store_list") ||
        !bind(resolved.remove, "fiaisdk_wallet_store_remove") ||
        !bind(resolved.random, "fiaisdk_crypto_random_bytes") ||
        !bind(resolved.keccak, "fiaisdk_crypto_keccak256") ||
        !bind(resolved.verify, "fiaisdk_crypto_verify")) {
        return library.errorString();
    }
    api_ = resolved;
    updateButtons();
    return {};
}

void WalletPanel::resetLibrary() {
    replaceWallet(nullptr);
    if (store_) {
        closeStore();
    }
    blsKey_->clear();
    secpKey_->clear();
    databaseKey_->clear();
    sdk_ = nullptr;
    api_ = {};
    updateButtons();
}

void WalletPanel::setSdk(const fiaisdk_sdk *sdk) {
    sdk_ = sdk;
    updateButtons();
}

void WalletPanel::replaceWallet(fiaisdk_wallet *wallet) {
    if (wallet_) {
        api_.destroy(wallet_);
    }
    wallet_ = wallet;
    updateButtons();
}

void WalletPanel::readInfo() {
    fiaisdk_wallet_info info = FIAISDK_WALLET_INFO_INIT;
    const auto status = api_.info(wallet_, &info);
    logResult_("fiaisdk_wallet_get_info", status);
    if (status == FIAISDK_STATUS_OK) {
        log_("secp256k1 address: " + QString::fromLatin1(hex(info.address)));
        log_("BLS address: " + QString::fromLatin1(hex(info.bls_address)));
        log_("secp256k1 public key: " + QString::fromLatin1(hex(info.secp256k1_public_key)));
        log_("BLS public key: " + QString::fromLatin1(hex(info.bls_public_key)));
    }
}

void WalletPanel::signAndVerify() {
    const auto scheme = scheme_->currentData().toInt();
    const bool bls = scheme == FIAISDK_CRYPTO_SCHEME_BLS12381;
    const auto message = message_->text().toUtf8();
    const auto *data = reinterpret_cast<const uint8_t *>(message.constData());
    std::size_t size = static_cast<std::size_t>(message.size());
    std::array<uint8_t, 32> digest{};
    if (!bls) {
        const auto status = api_.keccak(sdk_, data, size, digest.data(), digest.size());
        logResult_("fiaisdk_crypto_keccak256", status);
        if (status != FIAISDK_STATUS_OK) {
            return;
        }
        data = digest.data();
        size = digest.size();
    }
    std::array<uint8_t, 96> signature{};
    const std::size_t signatureSize = bls ? 96 : 65;
    const auto status =
        api_.sign(sdk_, wallet_, scheme, data, size, signature.data(), signatureSize);
    logResult_("fiaisdk_wallet_sign", status);
    if (status != FIAISDK_STATUS_OK) {
        return;
    }
    fiaisdk_wallet_info info = FIAISDK_WALLET_INFO_INIT;
    const auto read = api_.info(wallet_, &info);
    if (read != FIAISDK_STATUS_OK) {
        logResult_("fiaisdk_wallet_get_info", read);
        return;
    }
    const auto *publicKey = bls ? info.bls_public_key : info.secp256k1_public_key;
    const std::size_t publicKeySize =
        bls ? sizeof(info.bls_public_key) : sizeof(info.secp256k1_public_key);
    uint8_t valid = 0;
    const auto verified = api_.verify(sdk_, scheme, publicKey, publicKeySize, data, size,
                                      signature.data(), signatureSize, &valid);
    logResult_("fiaisdk_crypto_verify", verified);
    if (verified == FIAISDK_STATUS_OK) {
        log_(valid ? "Signature valid" : "Signature invalid");
    }
}

void WalletPanel::openStore() {
    const auto path = path_->text().toUtf8();
    auto key = readKey(databaseKey_);
    if (path.isEmpty() || key.isEmpty()) {
        log_("Enter a database path and a 64-character hex database key.");
        return;
    }
    const auto status = api_.open(
        path.constData(), reinterpret_cast<const uint8_t *>(key.constData()), key.size(), &store_);
    key.fill('\0');
    logResult_("fiaisdk_wallet_store_open", status);
    if (status == FIAISDK_STATUS_OK) {
        listWallets();
    }
    updateButtons();
}

void WalletPanel::closeStore() {
    api_.close(store_);
    store_ = nullptr;
    addresses_->clear();
    log_("fiaisdk_wallet_store_close completed");
    updateButtons();
}

void WalletPanel::listWallets() {
    const auto selected = addresses_->currentData();
    std::size_t count = 0;
    auto status = api_.list(store_, nullptr, &count);
    std::vector<fiaisdk_wallet_address> output;
    if (status == FIAISDK_STATUS_OK && count != 0) {
        output.resize(count);
        status = api_.list(store_, output.data(), &count);
    }
    logResult_("fiaisdk_wallet_store_list", status);
    if (status != FIAISDK_STATUS_OK) {
        return;
    }
    addresses_->clear();
    for (std::size_t i = 0; i < count; ++i) {
        const auto &address = output[i];
        const QByteArray bytes(reinterpret_cast<const char *>(address.bytes),
                               sizeof(address.bytes));
        addresses_->addItem(QString::fromLatin1(bytes.toHex()), bytes);
    }
    const int previous = addresses_->findData(selected);
    if (count != 0) {
        addresses_->setCurrentIndex(previous >= 0 ? previous : 0);
    }
    log_(QString("Stored wallets: %1").arg(count));
}

void WalletPanel::loadWallet() {
    const auto address = selectedAddress(addresses_);
    fiaisdk_wallet *wallet = nullptr;
    const auto status = api_.load(store_, sdk_, &address, &wallet);
    logResult_("fiaisdk_wallet_store_load", status);
    if (status == FIAISDK_STATUS_OK) {
        replaceWallet(wallet);
        readInfo();
    }
}

void WalletPanel::removeWallet() {
    const auto address = selectedAddress(addresses_);
    const auto status = api_.remove(store_, &address);
    logResult_("fiaisdk_wallet_store_remove", status);
    if (status == FIAISDK_STATUS_OK || status == FIAISDK_STATUS_NOT_FOUND) {
        listWallets();
    }
}

void WalletPanel::updateButtons() {
    const bool available = api_.create != nullptr;
    const bool hasSdk = available && sdk_;
    const bool hasStore = store_ != nullptr;
    const bool hasSelection = hasStore && addresses_->currentIndex() >= 0;
    create_->setEnabled(hasSdk);
    import_->setEnabled(hasSdk);
    blsKey_->setEnabled(hasSdk);
    secpKey_->setEnabled(hasSdk);
    info_->setEnabled(wallet_ != nullptr);
    destroy_->setEnabled(wallet_ != nullptr);
    sign_->setEnabled(hasSdk && wallet_);
    browse_->setEnabled(available && !hasStore);
    path_->setEnabled(available && !hasStore);
    databaseKey_->setEnabled(available && !hasStore);
    generateKey_->setEnabled(hasSdk && !hasStore);
    open_->setEnabled(available && !hasStore);
    close_->setEnabled(hasStore);
    save_->setEnabled(hasStore && wallet_);
    list_->setEnabled(hasStore);
    load_->setEnabled(hasSdk && hasSelection);
    remove_->setEnabled(hasSelection);
}
