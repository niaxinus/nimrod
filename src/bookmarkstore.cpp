#include "bookmarkstore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDebug>

BookmarkStore::BookmarkStore(QObject *parent)
    : QObject(parent)
    , m_connectionName("nimrod_bm_" + QUuid::createUuid().toString(QUuid::Id128))
{}

BookmarkStore::~BookmarkStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool BookmarkStore::init(const QString &dbPath)
{
    m_dbPath = dbPath;
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "BookmarkStore: DB megnyitási hiba:" << db.lastError().text();
        return false;
    }
    createSchema();
    return true;
}

void BookmarkStore::createSchema()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS bookmarks (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            title      TEXT    NOT NULL,
            url        TEXT    NOT NULL,
            folder     TEXT    NOT NULL DEFAULT '',
            position   INTEGER NOT NULL DEFAULT 0,
            date_added INTEGER NOT NULL DEFAULT 0
        )
    )");
    if (q.lastError().isValid())
        qWarning() << "BookmarkStore: séma hiba:" << q.lastError().text();
}

Bookmark BookmarkStore::add(const QString &title, const QString &url, const QString &folder)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    // Legmagasabb pozíció lekérése az adott mappában
    QSqlQuery posQ(db);
    posQ.prepare("SELECT COALESCE(MAX(position),0)+1 FROM bookmarks WHERE folder=:f");
    posQ.bindValue(":f", folder);
    posQ.exec();
    int pos = posQ.next() ? posQ.value(0).toInt() : 1;

    QSqlQuery q(db);
    q.prepare("INSERT INTO bookmarks (title,url,folder,position,date_added) VALUES (:t,:u,:f,:p,:d)");
    q.bindValue(":t", title);
    q.bindValue(":u", url);
    q.bindValue(":f", folder);
    q.bindValue(":p", pos);
    q.bindValue(":d", QDateTime::currentSecsSinceEpoch());
    q.exec();

    Bookmark bm;
    bm.id       = q.lastInsertId().toInt();
    bm.title    = title;
    bm.url      = url;
    bm.folder   = folder;
    bm.position = pos;
    bm.dateAdded = QDateTime::currentDateTime();

    emit changed();
    return bm;
}

void BookmarkStore::remove(int id)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM bookmarks WHERE id=:id");
    q.bindValue(":id", id);
    q.exec();
    emit changed();
}

void BookmarkStore::update(int id, const QString &title, const QString &url, const QString &folder)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE bookmarks SET title=:t, url=:u, folder=:f WHERE id=:id");
    q.bindValue(":t",  title);
    q.bindValue(":u",  url);
    q.bindValue(":f",  folder);
    q.bindValue(":id", id);
    q.exec();
    emit changed();
}

void BookmarkStore::move(int id, int newPosition)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE bookmarks SET position=:p WHERE id=:id");
    q.bindValue(":p",  newPosition);
    q.bindValue(":id", id);
    q.exec();
    emit changed();
}

static Bookmark rowToBookmark(QSqlQuery &q)
{
    Bookmark bm;
    bm.id        = q.value(0).toInt();
    bm.title     = q.value(1).toString();
    bm.url       = q.value(2).toString();
    bm.folder    = q.value(3).toString();
    bm.position  = q.value(4).toInt();
    bm.dateAdded = QDateTime::fromSecsSinceEpoch(q.value(5).toLongLong());
    return bm;
}

QList<Bookmark> BookmarkStore::all() const
{
    QList<Bookmark> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return result;
    QSqlQuery q("SELECT id,title,url,folder,position,date_added FROM bookmarks ORDER BY folder,position,id", db);
    while (q.next())
        result.append(rowToBookmark(q));
    return result;
}

QList<Bookmark> BookmarkStore::inFolder(const QString &folder) const
{
    QList<Bookmark> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return result;
    QSqlQuery q(db);
    q.prepare("SELECT id,title,url,folder,position,date_added FROM bookmarks WHERE folder=:f ORDER BY position,id");
    q.bindValue(":f", folder);
    q.exec();
    while (q.next())
        result.append(rowToBookmark(q));
    return result;
}

QStringList BookmarkStore::folders() const
{
    QStringList result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return result;
    QSqlQuery q("SELECT DISTINCT folder FROM bookmarks WHERE folder != '' ORDER BY folder", db);
    while (q.next())
        result.append(q.value(0).toString());
    return result;
}

bool BookmarkStore::hasUrl(const QString &url) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM bookmarks WHERE url=:u");
    q.bindValue(":u", url);
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
}

Bookmark BookmarkStore::byUrl(const QString &url) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id,title,url,folder,position,date_added FROM bookmarks WHERE url=:u LIMIT 1");
    q.bindValue(":u", url);
    q.exec();
    if (q.next())
        return rowToBookmark(q);
    return {};
}
