#include "utils/credentialstore.h"
#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("qEmby-security-wallet-test");
    if (app.arguments().size() != 2)
        return 2;
    const QString key = CredentialStore::key("integration-test", {"disposable-wallet-probe"});
    const QString value = QStringLiteral("disposable-qemby-wallet-test-not-a-real-token");
    const QString operation = app.arguments().at(1);
    if (operation == "write")
        return CredentialStore::write(key, value) ? 0 : 1;
    if (operation == "read")
        return CredentialStore::read(key) == value ? 0 : 1;
    if (operation == "delete")
        return CredentialStore::remove(key) ? 0 : 1;
    if (operation == "absent")
        return CredentialStore::read(key).isEmpty() ? 0 : 1;
    return 2;
}
