#include "nimrodbridge.h"
#include "credentialstore.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

NimrodBridge::NimrodBridge(CredentialStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{}

void NimrodBridge::saveCredential(const QString &domain,
                                   const QString &username,
                                   const QString &password)
{
    if (!m_store || domain.isEmpty() || username.isEmpty()) return;
    m_store->store(domain, username, password);
    qDebug() << "NimrodBridge: credential mentve:" << domain << username;
}

QString NimrodBridge::getCredentials(const QString &domain) const
{
    if (!m_store || domain.isEmpty()) return "[]";

    QList<Credential> creds = m_store->findForDomain(domain);
    QJsonArray arr;
    for (const Credential &c : creds) {
        QJsonObject obj;
        obj["username"] = c.username;
        obj["password"] = c.password;
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void NimrodBridge::removeCredential(const QString &domain, const QString &username)
{
    if (!m_store) return;
    m_store->remove(domain, username);
}
