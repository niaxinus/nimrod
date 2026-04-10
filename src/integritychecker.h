#pragma once

#include <QString>

// Induláskor ellenőrzi a futó bináris SHA-256 ujjlenyomatát.
// Az ujjlenyomatot AES-256-CBC-vel titkosítva tárolja SQLite-ban.
// Kulcs: SHA-256(linux_username) – ugyanaz mint a CredentialStore-ban.
class IntegrityChecker
{
public:
    // Elvégzi az ellenőrzést.
    // dbPath: a fingerprints adatbázis elérési útja
    // Visszatér: true = OK / első futás, false = eltérés észlelve
    static bool check(const QString &dbPath);

private:
    static QByteArray computeBinaryHash();
    static QByteArray deriveKey();
    static QString    encrypt(const QByteArray &data);
    static QByteArray decrypt(const QString &base64Cipher);
};
