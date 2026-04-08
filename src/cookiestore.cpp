#include "cookiestore.h"

#include <QWebEngineCookieStore>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QUuid>

CookieStore::CookieStore(QObject *parent)
    : QObject(parent)
    , m_connectionName("nimrod_cookies_" + QUuid::createUuid().toString(QUuid::Id128))
{}

CookieStore::~CookieStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool CookieStore::init(const QString &dbPath)
{
    m_dbPath = dbPath;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "CookieStore: nem sikerült megnyitni a DB-t:" << dbPath
                   << db.lastError().text();
        return false;
    }

    createSchema();
    qDebug() << "CookieStore: DB megnyitva:" << dbPath;
    return true;
}

void CookieStore::createSchema()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS cookies (
            id       INTEGER PRIMARY KEY AUTOINCREMENT,
            name     TEXT    NOT NULL,
            value    TEXT,
            domain   TEXT    NOT NULL,
            path     TEXT    NOT NULL DEFAULT '/',
            expiry   INTEGER DEFAULT 0,
            secure   INTEGER DEFAULT 0,
            httponly INTEGER DEFAULT 0,
            UNIQUE(name, domain, path)
        )
    )");

    if (q.lastError().isValid()) {
        qWarning() << "CookieStore: séma hiba:" << q.lastError().text();
    }
}

void CookieStore::loadInto(QWebEngineCookieStore *store)
{
    if (!store) return;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.exec("SELECT name, value, domain, path, expiry, secure, httponly FROM cookies");

    qint64 now = QDateTime::currentSecsSinceEpoch();
    int loaded = 0, skipped = 0;

    while (q.next()) {
        QNetworkCookie cookie;
        cookie.setName(q.value(0).toByteArray());
        cookie.setValue(q.value(1).toByteArray());

        QString domain = q.value(2).toString();
        cookie.setDomain(domain);
        cookie.setPath(q.value(3).toString());

        qint64 expiry = q.value(4).toLongLong();
        if (expiry > 0) {
            if (expiry < now) { skipped++; continue; } // lejárt
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(expiry));
        }

        cookie.setSecure(q.value(5).toInt() != 0);
        cookie.setHttpOnly(q.value(6).toInt() != 0);

        store->setCookie(cookie);
        loaded++;
    }

    qDebug() << "CookieStore: betöltve" << loaded << "cookie, kihagyva (lejárt)" << skipped;
}

void CookieStore::connectTo(QWebEngineCookieStore *store)
{
    if (!store) return;
    connect(store, &QWebEngineCookieStore::cookieAdded,
            this,  &CookieStore::onCookieAdded);
    connect(store, &QWebEngineCookieStore::cookieRemoved,
            this,  &CookieStore::onCookieRemoved);
}

void CookieStore::onCookieAdded(const QNetworkCookie &cookie)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return;

    qint64 expiry = 0;
    if (cookie.expirationDate().isValid())
        expiry = cookie.expirationDate().toSecsSinceEpoch();

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO cookies (name, value, domain, path, expiry, secure, httponly)
        VALUES (:name, :value, :domain, :path, :expiry, :secure, :httponly)
        ON CONFLICT(name, domain, path) DO UPDATE SET
            value    = excluded.value,
            expiry   = excluded.expiry,
            secure   = excluded.secure,
            httponly = excluded.httponly
    )");
    q.bindValue(":name",     QString::fromUtf8(cookie.name()));
    q.bindValue(":value",    QString::fromUtf8(cookie.value()));
    q.bindValue(":domain",   cookie.domain());
    q.bindValue(":path",     cookie.path());
    q.bindValue(":expiry",   expiry);
    q.bindValue(":secure",   cookie.isSecure() ? 1 : 0);
    q.bindValue(":httponly", cookie.isHttpOnly() ? 1 : 0);
    q.exec();
}

void CookieStore::onCookieRemoved(const QNetworkCookie &cookie)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("DELETE FROM cookies WHERE name=:name AND domain=:domain AND path=:path");
    q.bindValue(":name",   QString::fromUtf8(cookie.name()));
    q.bindValue(":domain", cookie.domain());
    q.bindValue(":path",   cookie.path());
    q.exec();
}

bool CookieStore::isExpired(const QNetworkCookie &cookie) const
{
    if (!cookie.expirationDate().isValid()) return false;
    return cookie.expirationDate() < QDateTime::currentDateTime();
}
