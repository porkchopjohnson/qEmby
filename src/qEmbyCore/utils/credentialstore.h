#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H

#include "../qEmbyCore_global.h"
#include <QString>
#include <QStringList>

// The OS wallet is the only persistent secret store. A denied/unavailable wallet
// leaves new credentials in process memory only; it never enables INI fallback.
namespace CredentialStore {
QEMBYCORE_EXPORT QString key(const QString &kind, const QStringList &identity);
QEMBYCORE_EXPORT QString read(const QString &key);
QEMBYCORE_EXPORT bool write(const QString &key, const QString &secret);
QEMBYCORE_EXPORT bool remove(const QString &key);
}

#endif
