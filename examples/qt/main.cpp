#include <fiaisdk/network.h>
#include <fiaisdk/sdk.h>

#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLibrary>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrentRun>

#include <limits>

namespace {

struct Api {
    decltype(&fiaisdk_version) version = nullptr;
    decltype(&fiaisdk_status_message) message = nullptr;
    decltype(&fiaisdk_sdk_create) create = nullptr;
    decltype(&fiaisdk_sdk_destroy) destroy = nullptr;
    decltype(&fiaisdk_network_connect) connect = nullptr;
    decltype(&fiaisdk_network_disconnect) disconnect = nullptr;
    decltype(&fiaisdk_network_get_status) status = nullptr;
    decltype(&fiaisdk_network_get_endpoint) endpoint = nullptr;

    QString resolve(QLibrary &library) {
        if (!bind(library, version, "fiaisdk_version") ||
            !bind(library, message, "fiaisdk_status_message") ||
            !bind(library, create, "fiaisdk_sdk_create") ||
            !bind(library, destroy, "fiaisdk_sdk_destroy") ||
            !bind(library, connect, "fiaisdk_network_connect") ||
            !bind(library, disconnect, "fiaisdk_network_disconnect") ||
            !bind(library, status, "fiaisdk_network_get_status") ||
            !bind(library, endpoint, "fiaisdk_network_get_endpoint")) {
            return library.errorString();
        }
        return {};
    }

  private:
    template <class Function>
    static bool bind(QLibrary &library, Function &function, const char *name) {
        function = reinterpret_cast<Function>(library.resolve(name));
        return function != nullptr;
    }
};

struct ConnectResult {
    fiaisdk_status status;
    qint64 elapsedMs;
};

class NetworkWindow final : public QWidget {
  public:
    explicit NetworkWindow(const QString &libraryPath) {
        setWindowTitle("FiaiSDK Network");
        resize(820, 510);

        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        layout->addLayout(form);

        auto *libraryRow = new QHBoxLayout;
        libraryPath_ = new QLineEdit(libraryPath);
        libraryPath_->setPlaceholderText("Path to fiaisdk.dll, libfiaisdk.dylib or libfiaisdk.so");
        libraryRow->addWidget(libraryPath_);
        browse_ = addButton(libraryRow, "Browse", [this] {
            const QString path = QFileDialog::getOpenFileName(
                this, "Select FiaiSDK library", libraryPath_->text(),
                "Shared libraries (*.dll *.dylib *.so *.so.*);;All files (*)");
            if (!path.isEmpty()) {
                libraryPath_->setText(path);
            }
        });
        load_ = addButton(libraryRow, "Load", [this] { loadLibrary(); });
        form->addRow("Library", libraryRow);

        endpoint_ = new QLineEdit("tcp://61.28.233.146:4200");
        form->addRow("Endpoint", endpoint_);
        timeout_ = new QSpinBox;
        timeout_->setRange(1, 60000);
        timeout_->setValue(5000);
        timeout_->setSuffix(" ms");
        form->addRow("Connect timeout", timeout_);

        auto *actions = new QHBoxLayout;
        layout->addLayout(actions);
        create_ = addButton(actions, "Create SDK", [this] { createSdk(); });
        destroy_ = addButton(actions, "Destroy SDK", [this] { destroySdk(); });
        connect_ = addButton(actions, "Connect", [this] { connectToEndpoint(); });
        disconnect_ = addButton(actions, "Disconnect", [this] {
            const auto status = api_.disconnect(sdk_);
            if (status == FIAISDK_STATUS_OK) {
                wasConnected_ = false;
            }
            logResult("fiaisdk_network_disconnect", status);
            readStatus();
        });
        status_ = addButton(actions, "Get status", [this] { readStatus(); });
        getEndpoint_ = addButton(actions, "Get endpoint", [this] {
            const auto endpoint = readEndpoint();
            log(endpoint.isEmpty() ? "Endpoint is empty" : "Endpoint: " + endpoint);
        });
        addButton(actions, "Clear log", [this] { log_->clear(); });

        layout->addWidget(new QLabel("Call log"));
        log_ = new QPlainTextEdit;
        log_->setReadOnly(true);
        log_->setMaximumBlockCount(1000);
        layout->addWidget(log_, 1);

        QObject::connect(&connectWatcher_, &QFutureWatcher<ConnectResult>::finished, this, [this] {
            const auto result = connectWatcher_.result();
            logResult("fiaisdk_network_connect", result.status,
                      QString(" (%1 ms)").arg(result.elapsedMs));
            connecting_ = false;
            if (result.status == FIAISDK_STATUS_OK) {
                wasConnected_ = true;
            }
            updateButtons();
            readStatus();
        });
        QObject::connect(&statusTimer_, &QTimer::timeout, this, [this] {
            if (sdk_ && wasConnected_ && !connecting_) {
                readStatus(false);
            }
        });
        statusTimer_.start(500);
        updateButtons();
    }

    ~NetworkWindow() override {
        connectWatcher_.waitForFinished();
        if (sdk_) {
            api_.destroy(sdk_);
        }
        if (library_.isLoaded()) {
            library_.unload();
        }
    }

  private:
    template <class Action>
    QPushButton *addButton(QHBoxLayout *layout, const QString &label, Action action) {
        auto *button = new QPushButton(label);
        layout->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, this, action);
        return button;
    }

    void log(const QString &message) {
        log_->appendPlainText(QTime::currentTime().toString("HH:mm:ss.zzz") + "  " + message);
    }

    void logResult(const QString &operation, fiaisdk_status status, const QString &extra = {}) {
        log(QString("%1 -> %2 (%3)%4")
                .arg(operation)
                .arg(status)
                .arg(QString::fromUtf8(api_.message(status)), extra));
    }

    void updateButtons() {
        const bool canLoad = !sdk_ && !connecting_;
        libraryPath_->setEnabled(canLoad);
        browse_->setEnabled(canLoad);
        load_->setEnabled(canLoad);
        create_->setEnabled(api_.create && !sdk_);
        destroy_->setEnabled(sdk_ && !connecting_);
        connect_->setEnabled(sdk_ && !connecting_);
        disconnect_->setEnabled(sdk_ && !connecting_);
        status_->setEnabled(sdk_ != nullptr);
        getEndpoint_->setEnabled(sdk_ != nullptr);
        endpoint_->setEnabled(!connecting_);
        timeout_->setEnabled(!connecting_);
    }

    void loadLibrary() {
        const auto path = libraryPath_->text().trimmed();
        if (path.isEmpty()) {
            log("Select a shared library first.");
            return;
        }
        if (library_.isLoaded() && !library_.unload()) {
            log("Unload failed: " + library_.errorString());
            return;
        }
        api_ = {};
        library_.setFileName(path);
        if (!library_.load()) {
            log("Load failed: " + library_.errorString());
            updateButtons();
            return;
        }
        if (const QString error = api_.resolve(library_); !error.isEmpty()) {
            log("Required symbol missing: " + error);
            library_.unload();
            api_ = {};
        } else {
            log(QString("Loaded %1 (SDK %2)")
                    .arg(library_.fileName(), QString::fromUtf8(api_.version())));
        }
        updateButtons();
    }

    void createSdk() {
        const fiaisdk_sdk_config config{sizeof(fiaisdk_sdk_config), "fiaisdk-qt-example", nullptr};
        logResult("fiaisdk_sdk_create", api_.create(&config, &sdk_));
        updateButtons();
        if (sdk_) {
            readStatus();
        }
    }

    void destroySdk() {
        api_.destroy(sdk_);
        sdk_ = nullptr;
        wasConnected_ = false;
        log("fiaisdk_sdk_destroy completed");
        updateButtons();
    }

    void connectToEndpoint() {
        const QByteArray endpoint = endpoint_->text().trimmed().toUtf8();
        const auto timeout = static_cast<uint32_t>(timeout_->value());
        log(QString("Connecting (%1 ms timeout)…").arg(timeout));
        connecting_ = true;
        updateButtons();
        connectWatcher_.setFuture(
            QtConcurrent::run([connect = api_.connect, sdk = sdk_, endpoint, timeout] {
                QElapsedTimer timer;
                timer.start();
                const auto status = connect(sdk, endpoint.constData(), timeout);
                return ConnectResult{status, timer.elapsed()};
            }));
    }

    void readStatus(bool print = true) {
        fiaisdk_network_status current = FIAISDK_NETWORK_STATUS_INIT;
        const auto status = api_.status(sdk_, &current);
        if (status != FIAISDK_STATUS_OK) {
            if (print) {
                logResult("fiaisdk_network_get_status", status);
            }
            return;
        }
        if (current.state != FIAISDK_NETWORK_STATE_CONNECTED) {
            if (wasConnected_) {
                log("DISCONNECTED | Remote peer closed the connection");
            } else if (print) {
                log(connecting_ ? "DISCONNECTED (connect in progress)" : "DISCONNECTED");
            }
            wasConnected_ = false;
            return;
        }

        wasConnected_ = true;
        if (print) {
            const auto established = QDateTime::fromMSecsSinceEpoch(current.connected_at_unix_ms);
            log(QString("CONNECTED | %1 | %2 ms | %3")
                    .arg(readEndpoint())
                    .arg(current.latency_ms)
                    .arg(established.toString(Qt::ISODateWithMs)));
        }
    }

    QString readEndpoint() {
        size_t size = 0;
        auto status = api_.endpoint(sdk_, nullptr, &size);
        while (status == FIAISDK_STATUS_BUFFER_TOO_SMALL) {
            if (size == 0 || size > static_cast<size_t>(std::numeric_limits<qsizetype>::max())) {
                log("Invalid endpoint buffer size");
                return {};
            }
            QByteArray buffer(static_cast<qsizetype>(size), '\0');
            status = api_.endpoint(sdk_, buffer.data(), &size);
            if (status == FIAISDK_STATUS_OK) {
                return QString::fromUtf8(buffer.constData());
            }
        }
        logResult("fiaisdk_network_get_endpoint", status);
        return {};
    }

    QLibrary library_;
    Api api_;
    fiaisdk_sdk *sdk_ = nullptr;
    QFutureWatcher<ConnectResult> connectWatcher_;
    QTimer statusTimer_;
    bool connecting_ = false;
    bool wasConnected_ = false;

    QLineEdit *libraryPath_;
    QLineEdit *endpoint_;
    QSpinBox *timeout_;
    QPlainTextEdit *log_;
    QPushButton *browse_;
    QPushButton *load_;
    QPushButton *create_;
    QPushButton *destroy_;
    QPushButton *connect_;
    QPushButton *disconnect_;
    QPushButton *status_;
    QPushButton *getEndpoint_;
};

} // namespace

int main(int argc, char **argv) {
    QApplication application(argc, argv);
    QString libraryPath = application.arguments().value(1);
    if (libraryPath.isEmpty() && QFileInfo::exists(QStringLiteral(FIAISDK_STAGED_LIBRARY_PATH))) {
        libraryPath = QStringLiteral(FIAISDK_STAGED_LIBRARY_PATH);
    }
    NetworkWindow window(libraryPath);
    window.show();
    return application.exec();
}
