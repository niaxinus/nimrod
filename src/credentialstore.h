#pragma once

#include <QObject>
#include <QString>
#include <QList>

struct Credential {
    QString username;
    QString password; // plaintext (decrypt után)
};

class CredentialStore : public QObject
{
    Q_OBJECT

public:
    explicit CredentialStore(QObject *parent = nullptr);
    ~CredentialStore();

    bool init(const QString &dbPath);

    // Menti vagy frissíti a credential-t (password AES-sel titkosítva, kulcs = username)
    void store(const QString &domain, const QString &username, const QString &password);

    // Visszaadja a domain-hez tárolt összes credential-t (jelszó visszafejtve)
    QList<Credential> findForDomain(const QString &domain) const;

    // Törli a credential-t
    void remove(const QString &domain, const QString &username);

private:
    void createSchema();

    // AES-256-CBC titkosítás/visszafejtés (OpenSSL EVP)
    // Kulcs: SHA-256(username), IV: véletlenszerű 16 bájt
    // Kimenet: base64(iv[16] + ciphertext)
    static QString encrypt(const QString &plaintext, const QString &key);
    static QString decrypt(const QString &base64Cipher, const QString &key);

    QString m_connectionName;
    QString m_dbPath;
};
