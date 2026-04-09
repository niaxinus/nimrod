#pragma once

#include <QObject>
#include <QString>

class CredentialStore;

class NimrodBridge : public QObject
{
    Q_OBJECT

public:
    explicit NimrodBridge(CredentialStore *store, QObject *parent = nullptr);

    // JS-ből hívható: belépési adatok mentése
    Q_INVOKABLE void saveCredential(const QString &domain,
                                    const QString &username,
                                    const QString &password);

    // JS-ből hívható: domain-hez tárolt credentials JSON tömbként
    // Visszatérés: '[{"username":"...", "password":"..."}, ...]'
    Q_INVOKABLE QString getCredentials(const QString &domain) const;

    // JS-ből hívható: credential törlése
    Q_INVOKABLE void removeCredential(const QString &domain, const QString &username);

private:
    CredentialStore *m_store;
};
