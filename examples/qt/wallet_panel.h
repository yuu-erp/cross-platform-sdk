#pragma once

#include <fiaisdk/wallet.h>

#include <QWidget>

#include <functional>

class QComboBox;
class QLibrary;
class QLineEdit;
class QPushButton;

class WalletPanel final : public QWidget {
  public:
    using Log = std::function<void(const QString &)>;
    using LogResult = std::function<void(const QString &, fiaisdk_status)>;

    WalletPanel(Log log, LogResult logResult);
    QString resolve(QLibrary &library);
    void resetLibrary();
    void setSdk(const fiaisdk_sdk *sdk);

  private:
    void replaceWallet(fiaisdk_wallet *wallet);
    void readInfo();
    void signAndVerify();
    void openStore();
    void closeStore();
    void listWallets();
    void loadWallet();
    void removeWallet();
    void updateButtons();

    struct Api {
        decltype(&fiaisdk_wallet_create) create = nullptr;
        decltype(&fiaisdk_wallet_import) import = nullptr;
        decltype(&fiaisdk_wallet_get_info) info = nullptr;
        decltype(&fiaisdk_wallet_sign) sign = nullptr;
        decltype(&fiaisdk_wallet_destroy) destroy = nullptr;
        decltype(&fiaisdk_wallet_store_open) open = nullptr;
        decltype(&fiaisdk_wallet_store_close) close = nullptr;
        decltype(&fiaisdk_wallet_store_save) save = nullptr;
        decltype(&fiaisdk_wallet_store_load) load = nullptr;
        decltype(&fiaisdk_wallet_store_list) list = nullptr;
        decltype(&fiaisdk_wallet_store_remove) remove = nullptr;
        decltype(&fiaisdk_crypto_random_bytes) random = nullptr;
        decltype(&fiaisdk_crypto_keccak256) keccak = nullptr;
        decltype(&fiaisdk_crypto_verify) verify = nullptr;
    };

    Log log_;
    LogResult logResult_;
    Api api_;
    const fiaisdk_sdk *sdk_ = nullptr;
    fiaisdk_wallet *wallet_ = nullptr;
    fiaisdk_wallet_store *store_ = nullptr;
    QLineEdit *blsKey_;
    QLineEdit *secpKey_;
    QLineEdit *message_;
    QLineEdit *path_;
    QLineEdit *databaseKey_;
    QComboBox *scheme_;
    QComboBox *addresses_;
    QPushButton *create_;
    QPushButton *import_;
    QPushButton *info_;
    QPushButton *sign_;
    QPushButton *destroy_;
    QPushButton *browse_;
    QPushButton *generateKey_;
    QPushButton *open_;
    QPushButton *close_;
    QPushButton *save_;
    QPushButton *list_;
    QPushButton *load_;
    QPushButton *remove_;
};
