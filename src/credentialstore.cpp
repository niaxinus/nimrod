#include "credentialstore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDebug>
#include <QByteArray>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <pwd.h>
#include <unistd.h>

static const int AES_KEY_LEN = 32; // AES-256
static const int AES_IV_LEN  = 16; // CBC IV

// Linux rendszer username lekérése (getpwuid biztonságosabb mint $USER)
static QString linuxUsername()
{
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name)
        return QString::fromLocal8Bit(pw->pw_name);
    // fallback: $USER env változó
    QByteArray user = qgetenv("USER");
    if (!user.isEmpty())
        return QString::fromLocal8Bit(user);
    return QStringLiteral("nimrod");
}

// SHA-256(linux_username) → 32 bájtos AES kulcs
static QByteArray deriveKey()
{
    QByteArray input = linuxUsername().toUtf8();
    QByteArray result(SHA256_DIGEST_LENGTH, '\0');
    SHA256(reinterpret_cast<const unsigned char*>(input.constData()),
           static_cast<size_t>(input.size()),
           reinterpret_cast<unsigned char*>(result.data()));
    return result;
}

CredentialStore::CredentialStore(QObject *parent)
    : QObject(parent)
    , m_connectionName("nimrod_cred_" + QUuid::createUuid().toString(QUuid::Id128))
{}

CredentialStore::~CredentialStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool CredentialStore::init(const QString &dbPath)
{
    m_dbPath = dbPath;
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "CredentialStore: DB megnyitási hiba:" << db.lastError().text();
        return false;
    }
    createSchema();
    return true;
}

void CredentialStore::createSchema()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS credentials (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            domain            TEXT NOT NULL,
            username          TEXT NOT NULL,
            encrypted_password TEXT NOT NULL,
            UNIQUE(domain, username)
        )
    )");
    if (q.lastError().isValid())
        qWarning() << "CredentialStore: séma hiba:" << q.lastError().text();
}

void CredentialStore::store(const QString &domain, const QString &username, const QString &password)
{
    if (domain.isEmpty() || username.isEmpty()) return;

    QString enc = encrypt(password);
    if (enc.isEmpty()) {
        qWarning() << "CredentialStore: titkosítás sikertelen";
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO credentials (domain, username, encrypted_password)
        VALUES (:domain, :username, :enc)
        ON CONFLICT(domain, username) DO UPDATE SET encrypted_password = excluded.encrypted_password
    )");
    q.bindValue(":domain",   domain);
    q.bindValue(":username", username);
    q.bindValue(":enc",      enc);
    if (!q.exec())
        qWarning() << "CredentialStore: mentési hiba:" << q.lastError().text();
    else
        qDebug() << "CredentialStore: elmentve:" << domain << username;
}

QList<Credential> CredentialStore::findForDomain(const QString &domain) const
{
    QList<Credential> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return result;

    QSqlQuery q(db);
    q.prepare("SELECT username, encrypted_password FROM credentials WHERE domain = :domain");
    q.bindValue(":domain", domain);
    q.exec();

    while (q.next()) {
        QString username = q.value(0).toString();
        QString enc      = q.value(1).toString();
        QString password = decrypt(enc);
        result.append({username, password});
    }
    return result;
}

void CredentialStore::remove(const QString &domain, const QString &username)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM credentials WHERE domain=:domain AND username=:username");
    q.bindValue(":domain",   domain);
    q.bindValue(":username", username);
    q.exec();
}

// ── AES-256-CBC titkosítás ─────────────────────────────────────────────────

QString CredentialStore::encrypt(const QString &plaintext)
{
    QByteArray keyBytes = deriveKey();  // SHA-256(linux_username)
    QByteArray iv(AES_IV_LEN, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), AES_IV_LEN) != 1)
        return {};

    QByteArray input = plaintext.toUtf8();
    QByteArray output(input.size() + AES_IV_LEN * 2, '\0'); // padding helye

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    int len = 0, totalLen = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(keyBytes.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(output.data()), &len,
                          reinterpret_cast<const unsigned char*>(input.constData()), input.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = len;
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(output.data() + totalLen), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    EVP_CIPHER_CTX_free(ctx);

    output.resize(totalLen);
    // Formátum: base64(iv + ciphertext)
    return QString::fromLatin1((iv + output).toBase64());
}

QString CredentialStore::decrypt(const QString &base64Cipher)
{
    QByteArray keyBytes = deriveKey();
    QByteArray raw = QByteArray::fromBase64(base64Cipher.toLatin1());

    if (raw.size() < AES_IV_LEN) return {};

    QByteArray iv     = raw.left(AES_IV_LEN);
    QByteArray cipher = raw.mid(AES_IV_LEN);

    QByteArray output(cipher.size() + AES_IV_LEN, '\0');

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    int len = 0, totalLen = 0;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(keyBytes.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(output.data()), &len,
                          reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = len;
    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(output.data() + totalLen), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    EVP_CIPHER_CTX_free(ctx);

    output.resize(totalLen);
    return QString::fromUtf8(output);
}
