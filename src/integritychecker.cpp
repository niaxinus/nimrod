#include "integritychecker.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <pwd.h>
#include <unistd.h>

static const int IC_KEY_LEN = 32;
static const int IC_IV_LEN  = 16;

// ── Linux username (ugyanaz mint credentialstore.cpp-ben) ─────────────────

static QString linuxUsername()
{
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name)
        return QString::fromLocal8Bit(pw->pw_name);
    QByteArray user = qgetenv("USER");
    if (!user.isEmpty())
        return QString::fromLocal8Bit(user);
    return QStringLiteral("nimrod");
}

QByteArray IntegrityChecker::deriveKey()
{
    QByteArray input = linuxUsername().toUtf8();
    QByteArray result(SHA256_DIGEST_LENGTH, '\0');
    SHA256(reinterpret_cast<const unsigned char*>(input.constData()),
           static_cast<size_t>(input.size()),
           reinterpret_cast<unsigned char*>(result.data()));
    return result;
}

// ── Bináris SHA-256 kiszámítása (/proc/self/exe) ──────────────────────────

QByteArray IntegrityChecker::computeBinaryHash()
{
    // /proc/self/exe = a futó bináris szimbolikus linkje – leggyorsabb módszer
    QFile f("/proc/self/exe");
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "IntegrityChecker: nem sikerült megnyitni /proc/self/exe";
        return {};
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return {};

    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    constexpr int CHUNK = 65536; // 64 KB-os blokkok
    while (!f.atEnd()) {
        QByteArray chunk = f.read(CHUNK);
        EVP_DigestUpdate(ctx,
            reinterpret_cast<const unsigned char*>(chunk.constData()),
            static_cast<size_t>(chunk.size()));
    }
    f.close();

    QByteArray hash(EVP_MD_size(EVP_sha256()), '\0');
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(hash.data()), &len);
    EVP_MD_CTX_free(ctx);

    return hash;
}

// ── AES-256-CBC titkosítás / visszafejtés ────────────────────────────────

QString IntegrityChecker::encrypt(const QByteArray &data)
{
    QByteArray keyBytes = deriveKey();
    QByteArray iv(IC_IV_LEN, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), IC_IV_LEN) != 1)
        return {};

    QByteArray output(data.size() + IC_IV_LEN * 2, '\0');

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    int len = 0, total = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
        reinterpret_cast<const unsigned char*>(keyBytes.constData()),
        reinterpret_cast<const unsigned char*>(iv.constData()));
    EVP_EncryptUpdate(ctx,
        reinterpret_cast<unsigned char*>(output.data()), &len,
        reinterpret_cast<const unsigned char*>(data.constData()), data.size());
    total = len;
    EVP_EncryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(output.data() + total), &len);
    total += len;
    EVP_CIPHER_CTX_free(ctx);

    output.resize(total);
    return QString::fromLatin1((iv + output).toBase64());
}

QByteArray IntegrityChecker::decrypt(const QString &base64Cipher)
{
    QByteArray keyBytes = deriveKey();
    QByteArray raw = QByteArray::fromBase64(base64Cipher.toLatin1());
    if (raw.size() < IC_IV_LEN) return {};

    QByteArray iv     = raw.left(IC_IV_LEN);
    QByteArray cipher = raw.mid(IC_IV_LEN);
    QByteArray output(cipher.size() + IC_IV_LEN, '\0');

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    int len = 0, total = 0;
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
    total = len;
    if (EVP_DecryptFinal_ex(ctx,
            reinterpret_cast<unsigned char*>(output.data() + total), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    total += len;
    EVP_CIPHER_CTX_free(ctx);

    output.resize(total);
    return output;
}

// ── Fő ellenőrző logika ───────────────────────────────────────────────────

bool IntegrityChecker::check(const QString &dbPath)
{
    // 1. Bináris hash kiszámítása
    QByteArray currentHash = computeBinaryHash();
    if (currentHash.isEmpty()) {
        qWarning() << "IntegrityChecker: hash számítás sikertelen";
        return true; // nem blokkolunk ha nem tudunk hashelni
    }

    // 2. DB megnyitása
    QString connName = "nimrod_integrity_" + QUuid::createUuid().toString(QUuid::Id128);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "IntegrityChecker: DB megnyitási hiba:" << db.lastError().text();
        return true;
    }

    // 3. Séma létrehozása
    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS fingerprints (
            id          INTEGER PRIMARY KEY,
            fingerprint TEXT    NOT NULL,
            updated_at  INTEGER NOT NULL DEFAULT 0
        )
    )");

    // 4. Tárolt ujjlenyomat lekérése
    q.exec("SELECT fingerprint FROM fingerprints WHERE id=1");
    bool matched = true;

    if (q.next()) {
        // Van tárolt ujjlenyomat – összehasonlítás
        QString storedEnc = q.value(0).toString();
        QByteArray storedHash = decrypt(storedEnc);

        matched = (storedHash == currentHash);

        if (!matched) {
            qWarning() << "IntegrityChecker: ELTÉRÉS ÉSZLELVE! A bináris megváltozott.";
        }

        // Frissítés az aktuálisra
        QString newEnc = encrypt(currentHash);
        QSqlQuery upd(db);
        upd.prepare("UPDATE fingerprints SET fingerprint=:f, updated_at=:t WHERE id=1");
        upd.bindValue(":f", newEnc);
        upd.bindValue(":t", QDateTime::currentSecsSinceEpoch());
        upd.exec();
    } else {
        // Első futás – mentés
        QString enc = encrypt(currentHash);
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO fingerprints (id, fingerprint, updated_at) VALUES (1, :f, :t)");
        ins.bindValue(":f", enc);
        ins.bindValue(":t", QDateTime::currentSecsSinceEpoch());
        ins.exec();
        qDebug() << "IntegrityChecker: Első futás, ujjlenyomat elmentve.";
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);

    return matched;
}
