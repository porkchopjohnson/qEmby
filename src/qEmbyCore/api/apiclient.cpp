#include "apiclient.h"

#include <utility>

ApiClient::ApiClient(const ServerProfile& profile, NetworkManager* nm, QObject* parent)
    : QObject(parent), m_profile(profile), m_network(nm) {}

QMap<QString, QString> ApiClient::getAuthHeaders() const {
    QMap<QString, QString> headers;

    
    
    const QString scheme = m_profile.type == ServerProfile::Emby
                               ? QStringLiteral("Emby") : QStringLiteral("MediaBrowser");
    QString auth = QString("%2 Client=\"qEmby\", Device=\"Desktop\", "
                           "DeviceId=\"%1\", Version=\"0.1\"")
                       .arg(m_profile.deviceId, scheme);

    if (!m_profile.accessToken.isEmpty()) {
        headers.insert("X-Emby-Token", m_profile.accessToken);
        if (m_profile.type == ServerProfile::Jellyfin)
            auth += QString(", Token=\"%1\"").arg(m_profile.accessToken);
    }

    
    headers.insert("Authorization", auth);
    return headers;
}

NetworkRequestOptions ApiClient::requestOptions() const {
    NetworkRequestOptions options;
    options.ignoreSslErrors = m_profile.ignoreSslVerification;
    options.userAgentServerId = m_profile.id;
    return options;
}

QCoro::Task<QJsonObject> ApiClient::get(const QString& path) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->get(fullUrl, getAuthHeaders(),
                                      requestOptions());
}

QCoro::Task<QString> ApiClient::getText(const QString& path) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->getText(fullUrl, getAuthHeaders(),
                                          requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::post(const QString& path, const QJsonObject& payload) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->post(fullUrl, getAuthHeaders(), payload,
                                       requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::postArray(const QString& path, const QJsonArray& payload) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->postArray(fullUrl, getAuthHeaders(), payload,
                                            requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::postBytes(const QString& path,
                                              QByteArray payload,
                                              QString contentType) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->postBytes(fullUrl, getAuthHeaders(),
                                            std::move(payload),
                                            std::move(contentType),
                                            requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::postForm(const QString& path,
                                             const QUrlQuery& formData) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->postForm(fullUrl, getAuthHeaders(), formData,
                                           requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::deleteResource(const QString& path) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->deleteResource(fullUrl, getAuthHeaders(),
                                                 requestOptions());
}
