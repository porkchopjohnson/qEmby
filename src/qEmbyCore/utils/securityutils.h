#ifndef SECURITYUTILS_H
#define SECURITYUTILS_H

#include "../qEmbyCore_global.h"
#include <QString>
#include <QUrl>

namespace SecurityUtils {
QEMBYCORE_EXPORT QString safeUrl(QUrl url);
QEMBYCORE_EXPORT QString redactLogMessage(QString message);
QEMBYCORE_EXPORT bool isSafeServerUrl(const QUrl &url);
}

#endif
