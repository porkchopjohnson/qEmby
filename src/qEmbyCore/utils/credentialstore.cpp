#include "credentialstore.h"

#include <qt6keychain/keychain.h>
#include <QCryptographicHash>
#include <QDebug>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

namespace {
constexpr auto kService = "qEmby-security";
QHash<QString, QString> sessionSecrets;
QHash<QString, QString> persistedSecrets;

bool run(QKeychain::Job &job, bool allowMissing = false)
{
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool finished = false;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, [&] {
        finished = true;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(30000);
    job.start();
    if (!finished)
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (!finished) {
        qWarning() << "[CredentialStore] wallet timed out; no plaintext fallback";
        return false;
    }
    if (job.error() != QKeychain::NoError &&
        !(allowMissing && job.error() == QKeychain::EntryNotFound)) {
        qWarning() << "[CredentialStore] wallet operation failed or access denied; no plaintext fallback";
        return false;
    }
    return true;
}
}

QString CredentialStore::key(const QString &kind, const QStringList &identity)
{
    const QByteArray encoded = QJsonDocument(QJsonArray::fromStringList(identity)).toJson(QJsonDocument::Compact);
    return kind + QLatin1Char('/') + QString::fromLatin1(
        QCryptographicHash::hash(encoded, QCryptographicHash::Sha256).toHex());
}

QString CredentialStore::read(const QString &key)
{
    if (sessionSecrets.contains(key))
        return sessionSecrets.value(key);
    QKeychain::ReadPasswordJob job(QString::fromLatin1(kService));
    job.setKey(key);
    if (!run(job, true))
        return {};
    const QString value = job.error() == QKeychain::EntryNotFound ? QString() : job.textData();
    sessionSecrets.insert(key, value);
    persistedSecrets.insert(key, value);
    return value;
}

bool CredentialStore::write(const QString &key, const QString &secret)
{
    sessionSecrets.insert(key, secret);
    if (persistedSecrets.contains(key) && persistedSecrets.value(key) == secret)
        return true;
    if (secret.isEmpty())
        return remove(key);
    QKeychain::WritePasswordJob job(QString::fromLatin1(kService));
    job.setKey(key);
    job.setTextData(secret);
    if (!run(job))
        return false;
    persistedSecrets.insert(key, secret);
    return true;
}

bool CredentialStore::remove(const QString &key)
{
    sessionSecrets.insert(key, QString());
    if (persistedSecrets.contains(key) && persistedSecrets.value(key).isEmpty())
        return true;
    persistedSecrets.remove(key);
    QKeychain::DeletePasswordJob job(QString::fromLatin1(kService));
    job.setKey(key);
    if (!run(job, true))
        return false;
    persistedSecrets.insert(key, QString());
    return true;
}
