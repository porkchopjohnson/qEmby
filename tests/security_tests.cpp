#include "api/networkmanager.h"
#include "services/manager/servermanager.h"
#include "services/auth/authservice.h"
#include "utils/credentialstore.h"
#include "utils/securityutils.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

// A strict Emby fixture: require the documented headers, preserve /emby base
// paths, and reject admin-only system information for an ordinary media user.
class MockEmbyServer : public QTcpServer {
public:
    bool rejectDiscovery = false;
    bool rejectLogin = false;
    bool sawLogin = false;
    bool sawUserValidation = false;
    bool sawAdminRequest = false;

    MockEmbyServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
                socket->setProperty("buffer", buffer);
                const int headerEnd = buffer.indexOf("\r\n\r\n");
                if (headerEnd < 0)
                    return;
                const auto lines = buffer.left(headerEnd).split('\n');
                QMap<QByteArray, QByteArray> headers;
                for (const auto &line : lines.mid(1)) {
                    const int colon = line.indexOf(':');
                    if (colon > 0)
                        headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
                }
                const int length = headers.value("content-length").toInt();
                if (buffer.size() < headerEnd + 4 + length)
                    return;
                const auto request = lines.first().split(' ');
                const QByteArray path = request.value(1);
                const bool identified = headers.value("authorization").startsWith("Emby Client=\"qEmby\"") &&
                                        headers.value("authorization").contains("DeviceId=\"");
                int status = 401;
                QByteArray body = "{}";
                if (path == "/emby/System/Info/Public" && identified && !rejectDiscovery) {
                    status = 200;
                    body = "{\"ProductName\":\"Emby Server\",\"ServerName\":\"Test Server\"}";
                } else if (path == "/emby/Users/AuthenticateByName") {
                    sawLogin = true;
                    const auto payload = QJsonDocument::fromJson(buffer.mid(headerEnd + 4, length)).object();
                    if (identified && !rejectLogin && payload["Username"] == "test-user" && payload["Pw"] == "test-password") {
                        status = 200;
                        body = "{\"AccessToken\":\"test-session-token\",\"User\":{\"Id\":\"test-user\",\"Name\":\"test-user\",\"Policy\":{\"IsAdministrator\":false}}}";
                    }
                } else if (path == "/emby/Users/test-user") {
                    sawUserValidation = true;
                    if (identified && headers.value("x-emby-token") == "test-session-token") {
                        status = 200;
                        body = "{\"Id\":\"test-user\",\"Name\":\"test-user\",\"Policy\":{\"IsAdministrator\":false}}";
                    }
                } else if (path == "/emby/System/Info") {
                    sawAdminRequest = true;
                } else if (path.startsWith("/emby/web/")) {
                    status = 200;
                    body = "test-icon";
                }
                socket->write("HTTP/1.1 " + QByteArray::number(status) +
                              (status == 200 ? " OK\r\n" : " Unauthorized\r\n") +
                              "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }
    QString url() const { return QString("http://127.0.0.1:%1/emby").arg(serverPort()); }
};

class SecurityTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_home;
private slots:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        qputenv("XDG_DATA_HOME", m_home.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", m_home.path().toUtf8());
        QCoreApplication::setOrganizationName("qemby-security-tests");
        QCoreApplication::setApplicationName("qemby-security-tests");
    }
    void urlPolicy_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("tls") << "https://media.example.com/emby" << true;
        QTest::newRow("public-http") << "http://media.example.com" << false;
        QTest::newRow("public-ip") << "http://8.8.8.8" << false;
        QTest::newRow("loopback") << "http://127.0.0.1:8096" << true;
        QTest::newRow("lan") << "http://192.168.1.2:8096" << true;
        QTest::newRow("private-v6") << "http://[fd00::1]:8096" << true;
        QTest::newRow("mapped-public-v6") << "http://[::ffff:8.8.8.8]" << false;
        QTest::newRow("userinfo") << "https://user:secret@media.example.com" << false;
        QTest::newRow("query") << "https://media.example.com/?token=secret" << false;
        QTest::newRow("file") << "file:///etc/passwd" << false;
    }
    void urlPolicy()
    {
        QFETCH(QString, url);
        QFETCH(bool, allowed);
        QCOMPARE(SecurityUtils::isSafeServerUrl(QUrl(url)), allowed);
    }
    void redactCredentials()
    {
        const QString log = QStringLiteral(
            "connect wss://user:topsecret@example.com/socket?api_key=tokensecret&deviceId=id "
            "https://example.com/pathsecret/api/v2/search?q=secret "
            "Token=\"headersecret\", accessToken: jsonsecret");
        const QString clean = SecurityUtils::redactLogMessage(log);
        for (const QString &secret : {"topsecret", "tokensecret", "pathsecret", "headersecret", "jsonsecret"})
            QVERIFY2(!clean.contains(secret), qPrintable(clean));
        QVERIFY(clean.contains("example.com/socket"));
        QVERIFY(!clean.contains("deviceId"));
    }
    void walletIdentityBinding()
    {
        const QString key = CredentialStore::key("server-token", {"id", "https://one", "user"});
        QVERIFY(key != CredentialStore::key("server-token", {"id", "https://two", "user"}));
        QVERIFY(key != CredentialStore::key("server-token", {"id", "https://one", "other-user"}));
        QVERIFY(key != CredentialStore::key("server-proxy", {"id", "https://one", "user"}));
    }
    void noPlaintextWhenWalletUnavailable()
    {
        NetworkManager network;
        ServerManager manager(&network);
        ServerProfile profile;
        profile.id = "regression-test";
        profile.url = "https://example.com";
        profile.userId = "test-user";
        profile.accessToken = "NEVER-WRITE-THIS-TOKEN";
        profile.proxy.password = "NEVER-WRITE-THIS-PASSWORD";
        manager.addServer(profile);
        QFile settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/servers.json");
        QVERIFY(settings.open(QIODevice::ReadOnly));
        const QByteArray data = settings.readAll();
        QVERIFY(!data.contains("NEVER-WRITE"));
        QVERIFY(!data.contains("accessToken"));
        QVERIFY(!data.contains("password"));
        QCOMPARE(settings.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                 QFileDevice::ReadOther | QFileDevice::WriteOther), QFileDevice::Permissions());
        QCOMPARE(manager.servers().first().accessToken, profile.accessToken);
        manager.removeServer(profile.id);

        // Old upstream settings must be scrubbed even when migration cannot
        // reach a wallet. The imported session remains usable in memory.
        settings.close();
        QVERIFY(settings.open(QIODevice::WriteOnly | QIODevice::Truncate));
        settings.write("[{\"id\":\"legacy\",\"url\":\"https://example.com\",\"userId\":\"test-user\","
                       "\"accessToken\":\"LEGACY-TOKEN\",\"proxy\":{\"password\":\"LEGACY-PASSWORD\"}}]");
        settings.close();
        manager.loadSettings();
        QCOMPARE(manager.servers().first().accessToken, QString("LEGACY-TOKEN"));
        QVERIFY(settings.open(QIODevice::ReadOnly));
        const QByteArray migrated = settings.readAll();
        QVERIFY(!migrated.contains("LEGACY-TOKEN"));
        QVERIFY(!migrated.contains("LEGACY-PASSWORD"));
        manager.removeServer("legacy");
    }
    void crossOriginRedirectIsNotFollowed()
    {
        QTcpServer first, second;
        QVERIFY(first.listen(QHostAddress::LocalHost));
        QVERIFY(second.listen(QHostAddress::LocalHost));
        connect(&first, &QTcpServer::newConnection, &first, [&] {
            auto socket = first.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                socket->write("HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:" +
                              QByteArray::number(second.serverPort()) +
                              "/stolen\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
            });
        });
        QNetworkAccessManager network;
        QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/login").arg(first.serverPort())));
        request.setRawHeader("X-Emby-Authorization", "MediaBrowser Token=secret");
        NetworkManager::applyRequestOptions(request, {});
        auto reply = network.get(request);
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));
        QCOMPARE(reply->error(), QNetworkReply::InsecureRedirectError);
        QVERIFY(!second.hasPendingConnections());
        reply->deleteLater();
    }
    void embyLoginAndOrdinaryUserSession()
    {
        MockEmbyServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        NetworkManager network;
        ServerManager manager(&network);
        AuthService auth(&network, &manager);
        const auto profile = QCoro::waitFor(auth.login(server.url(), "test-user", "test-password", false));
        QVERIFY(server.sawLogin);
        QCOMPARE(profile.accessToken, QString("test-session-token"));
        QVERIFY(!profile.isAdmin);
        QCoro::waitFor(auth.validateSession(profile.id));
        QVERIFY(server.sawUserValidation);
        QVERIFY(!server.sawAdminRequest);
        manager.removeServer(profile.id);
    }
    void embyLoginErrorsIdentifyStage_data()
    {
        QTest::addColumn<bool>("discovery");
        QTest::newRow("discovery-401") << true;
        QTest::newRow("login-401") << false;
    }
    void embyLoginErrorsIdentifyStage()
    {
        QFETCH(bool, discovery);
        MockEmbyServer server;
        server.rejectDiscovery = discovery;
        server.rejectLogin = !discovery;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        NetworkManager network;
        ServerManager manager(&network);
        AuthService auth(&network, &manager);
        QString message;
        try {
            QCoro::waitFor(auth.login(server.url(), "test-user", "test-password", false));
        } catch (const std::exception &e) {
            message = QString::fromUtf8(e.what());
        }
        QVERIFY(message.contains(discovery ? "/System/Info/Public" : "/Users/AuthenticateByName"));
        QVERIFY(message.contains("401"));
        QVERIFY(!message.contains("test-password"));
        QVERIFY(!message.contains("test-session-token"));
        QVERIFY(manager.servers().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SecurityTests)
#include "security_tests.moc"
