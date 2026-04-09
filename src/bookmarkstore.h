#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

struct Bookmark {
    int       id        = 0;
    QString   title;
    QString   url;
    QString   folder;     // "" = toolbar (alap szint)
    int       position  = 0;
    QDateTime dateAdded;
};

class BookmarkStore : public QObject
{
    Q_OBJECT

public:
    explicit BookmarkStore(QObject *parent = nullptr);
    ~BookmarkStore();

    bool init(const QString &dbPath);

    // CRUD
    Bookmark    add(const QString &title, const QString &url, const QString &folder = "");
    void        remove(int id);
    void        update(int id, const QString &title, const QString &url, const QString &folder);
    void        move(int id, int newPosition);

    // Lekérdezések
    QList<Bookmark> all() const;
    QList<Bookmark> inFolder(const QString &folder) const;
    QStringList     folders() const;
    bool            hasUrl(const QString &url) const;
    Bookmark        byUrl(const QString &url) const;

signals:
    void changed();

private:
    void createSchema();

    QString m_connectionName;
    QString m_dbPath;
};
