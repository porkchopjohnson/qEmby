#include "authservice.h"
#include "utils/securityutils.h"
#include <QJsonObject>
#include <QUuid>
#include <QUrl>
#include <qcoronetwork.h>
#include <stdexcept>

namespace {

bool resolveContentDownloadPermission(const QJsonObject& policyObj)
{
    return policyObj["EnableContentDownloading"].toBool(
        policyObj["EnableMediaDownloading"].toBool(true));
}

} 

AuthService::AuthService(NetworkManager* networkManager, ServerManager* serverManager, QObject *parent)
    : QObject(parent), m_networkManager(networkManager), m_serverManager(serverManager)
{
}

QCoro::Task<ServerProfile> AuthService::login(QString serverUrl,
                                              QString username,
                                              QString password,
                                              bool ignoreSslVerification)
{
    if (!SecurityUtils::isSafeServerUrl(QUrl(serverUrl)))
        throw std::runtime_error("Use HTTPS with a valid server address. HTTP is allowed only for localhost or private LAN IP addresses.");
    QString cleanUrl = serverUrl;
    while (cleanUrl.endsWith('/')) {
        cleanUrl.chop(1);
    }

    NetworkRequestOptions requestOptions;
    requestOptions.ignoreSslErrors = ignoreSslVerification;

    
    
    ServerProfile existingProfile;
    bool hasExistingProfile = false;
    for (const ServerProfile &savedProfile : m_serverManager->servers()) {
        QString savedUrl = savedProfile.url;
        while (savedUrl.endsWith(QLatin1Char('/'))) {
            savedUrl.chop(1);
        }
        if (savedUrl == cleanUrl &&
            savedProfile.userName.compare(username, Qt::CaseInsensitive) == 0) {
            existingProfile = savedProfile;
            hasExistingProfile = true;
            requestOptions.userAgentServerId = savedProfile.id;
            break;
        }
    }

    qDebug() << "[AuthService] Login start"
             << "| url:" << cleanUrl
             << "| ignoreSslVerification:" << ignoreSslVerification;

    
    
    QJsonObject infoResp;

    
    ServerProfile tempProfile;
    if (hasExistingProfile) {
        tempProfile.id = existingProfile.id;
        tempProfile.deviceId = existingProfile.deviceId;
    }
    tempProfile.url = cleanUrl;
    if (tempProfile.deviceId.isEmpty()) {
        tempProfile.deviceId =
            QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    tempProfile.ignoreSslVerification = ignoreSslVerification;

    // Identify the client even before login, without sending an old token.
    try {
        ApiClient discoveryClient(tempProfile, m_networkManager);
        infoResp = co_await discoveryClient.get("/System/Info/Public");
    } catch (const std::exception &e) {
        throw std::runtime_error(tr("Server discovery failed (/System/Info/Public): %1")
                                     .arg(QString::fromUtf8(e.what())).toStdString());
    }

    
    QString productName = infoResp["ProductName"].toString();
    if (productName.contains("Jellyfin", Qt::CaseInsensitive)) {
        tempProfile.type = ServerProfile::Jellyfin;
    } else {
        tempProfile.type = ServerProfile::Emby;
    }

    
    QString realName = infoResp["ServerName"].toString();
    if (!realName.isEmpty()) {
        tempProfile.name = realName;
    } else {
        QUrl parsedUrl(cleanUrl);
        tempProfile.name = parsedUrl.host() + (parsedUrl.port() != -1 ? ":" + QString::number(parsedUrl.port()) : "");
    }

    
    ApiClient tempClient(tempProfile, m_networkManager);
    QJsonObject payload;
    payload["Username"] = username;
    payload["Pw"] = password;

    
    QJsonObject authResp;
    try {
        authResp = co_await tempClient.post("/Users/AuthenticateByName", payload);
    } catch (const std::exception &e) {
        throw std::runtime_error(tr("Sign-in failed (/Users/AuthenticateByName): %1")
                                     .arg(QString::fromUtf8(e.what())).toStdString());
    }

    
    tempProfile.accessToken = authResp["AccessToken"].toString();

    QJsonObject userObj = authResp["User"].toObject();
    tempProfile.userId = userObj["Id"].toString();
    tempProfile.userName = userObj["Name"].toString();
    if (tempProfile.accessToken.isEmpty() || tempProfile.userId.isEmpty())
        throw std::runtime_error("The server returned an incomplete sign-in response (missing access token or user ID).");

    QJsonObject policyObj = userObj["Policy"].toObject();
    tempProfile.isAdmin = policyObj["IsAdministrator"].toBool();
    tempProfile.canDownloadMedia = resolveContentDownloadPermission(policyObj);

    
    
    {
        QStringList iconCandidates;
        if (tempProfile.type == ServerProfile::Jellyfin) {
            iconCandidates = {
                QStringLiteral("/web/icon-transparent.png"),
                QStringLiteral("/web/touchicon.png"),
                QStringLiteral("/web/favicon.png"),
                QStringLiteral("/web/favicon.ico"),
            };
        } else {
            iconCandidates = {
                QStringLiteral("/web/touchicon.png"),
                QStringLiteral("/web/touchicon144.png"),
                QStringLiteral("/web/favicon.png"),
                QStringLiteral("/web/favicon.ico"),
                QStringLiteral("/emby/web/touchicon.png"),
                QStringLiteral("/emby/web/favicon.png"),
            };
        }

        qDebug() << "[AuthService] Attempting to fetch server icon"
                 << "| type:" << (tempProfile.type == ServerProfile::Jellyfin ? "Jellyfin" : "Emby")
                 << "| candidates:" << iconCandidates.size();

        for (const QString &iconPath : iconCandidates) {
            try {
                const QByteArray iconBytes = co_await m_networkManager->getBytes(
                    tempProfile.url + iconPath, QMap<QString, QString>(),
                    requestOptions);
                if (!iconBytes.isEmpty()) {
                    tempProfile.iconBase64 =
                        QString::fromUtf8(iconBytes.toBase64());
                    qDebug() << "[AuthService] Server icon fetched successfully"
                             << "| path:" << iconPath
                             << "| size:" << iconBytes.size() << "bytes";
                    break;
                }
            } catch (const std::exception &e) {
                qDebug() << "[AuthService] Icon fetch failed, trying next"
                         << "| path:" << iconPath
                         << "| error:" << e.what();
            } catch (...) {
                qDebug() << "[AuthService] Icon fetch failed (unknown error)"
                         << "| path:" << iconPath;
            }
        }

        if (tempProfile.iconBase64.isEmpty()) {
            qWarning() << "[AuthService] All icon fetch attempts failed"
                       << "| url:" << tempProfile.url
                       << "| type:" << (tempProfile.type == ServerProfile::Jellyfin ? "Jellyfin" : "Emby");
        }
    }

    
    tempProfile = m_serverManager->addServer(tempProfile);
    m_serverManager->setActiveServer(tempProfile.id);

    
    co_return tempProfile;
}

void AuthService::logout()
{
    
    m_serverManager->clearActiveSession();

    
    Q_EMIT userLoggedOut();
}

QCoro::Task<ServerProfile> AuthService::validateSession(const QString& serverId)
{
    
    QList<ServerProfile> servers = m_serverManager->servers();
    ServerProfile targetProfile;
    bool found = false;
    for (const auto& p : servers) {
        if (p.id == serverId) {
            targetProfile = p;
            found = true;
            break;
        }
    }

    if (!found) {
        throw std::runtime_error(tr("本地存储中找不到该服务器配置。").toStdString());
    }

    
    if (targetProfile.accessToken.isEmpty())
        throw std::runtime_error("No saved login token is available. Unlock KDE Wallet or sign in again.");
    ApiClient tempClient(targetProfile, m_networkManager);

    
    
    // Check the user's session, not an endpoint that may require admin access.
    QJsonObject userResp;
    try {
        userResp = co_await tempClient.get(QStringLiteral("/Users/%1").arg(targetProfile.userId));
    } catch (const std::exception &e) {
        throw std::runtime_error(tr("Saved session validation failed (/Users/{id}): %1")
                                     .arg(QString::fromUtf8(e.what())).toStdString());
    }
    const QJsonObject policyObj = userResp.value(QStringLiteral("Policy")).toObject();
    targetProfile.userName = userResp.value(QStringLiteral("Name")).toString(
        targetProfile.userName);
    targetProfile.isAdmin = policyObj.value(QStringLiteral("IsAdministrator"))
                                .toBool(targetProfile.isAdmin);
    targetProfile.canDownloadMedia = resolveContentDownloadPermission(policyObj);
    m_serverManager->addServer(targetProfile);

    
    m_serverManager->setActiveServer(targetProfile.id);

    co_return targetProfile;
}
