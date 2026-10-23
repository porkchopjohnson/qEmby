#include "securityutils.h"

#include <QHostAddress>
#include <QRegularExpression>

QString SecurityUtils::safeUrl(QUrl url)
{
    if (!url.isValid())
        return QStringLiteral("<invalid-url>");
    url.setUserInfo(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    // Some danmu_api deployments put their authentication key in the path.
    QString path = url.path();
    const int apiIndex = path.indexOf(QStringLiteral("/api/v2/"), 0, Qt::CaseInsensitive);
    if (apiIndex > 0) {
        const int slash = path.lastIndexOf('/', apiIndex - 1);
        path.replace(slash + 1, apiIndex - slash - 1, QStringLiteral("<redacted>"));
        url.setPath(path);
    }
    static const QRegularExpression relayPath(QStringLiteral("^/[^/]+/stream$"));
    if (QHostAddress(url.host()).isLoopback() && relayPath.match(path).hasMatch())
        url.setPath(QStringLiteral("/<redacted>/stream"));
    return url.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
}

QString SecurityUtils::redactLogMessage(QString message)
{
    static const QRegularExpression urls(QStringLiteral(R"((?:https?|wss?|socks5)://[^\s"'<>]+)"),
                                         QRegularExpression::CaseInsensitiveOption);
    auto matches = urls.globalMatch(message);
    QString result;
    qsizetype end = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += message.mid(end, match.capturedStart() - end);
        result += safeUrl(QUrl(match.captured()));
        end = match.capturedEnd();
    }
    result += message.mid(end);
    static const QRegularExpression secrets(
        QStringLiteral(R"((\b(?:api_key|api_token|access_token|accessToken|X-Emby-Token|password|Pw|Token)\b["']?\s*[:=]\s*["']?)[^\s,;&"'}]+)"),
        QRegularExpression::CaseInsensitiveOption);
    result.replace(secrets, QStringLiteral("\\1<redacted>"));
    return result;
}

bool SecurityUtils::isSafeServerUrl(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() ||
        url.hasQuery() || url.hasFragment())
        return false;
    if (url.scheme() == QStringLiteral("https"))
        return true;
    if (url.scheme() != QStringLiteral("http"))
        return false;
    // Keep local media servers usable, but never send credentials over public
    // plaintext HTTP. Host names other than localhost require HTTPS.
    if (url.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0)
        return true;
    const QHostAddress host(url.host());
    return host.isLoopback() || host.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
           host.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
           host.isInSubnet(QHostAddress("192.168.0.0"), 16) ||
           host.isInSubnet(QHostAddress("fc00::"), 7);
}
