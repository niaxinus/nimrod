#pragma once

#include <QObject>
#include <QString>
#include <QNetworkCookie>

class QWebEngineCookieStore;
class QSqlDatabase;

/**
 * CookieStore
 *
 * SQLite alapú cookie tár: ~/.config/nimrod/cookies.db
 *
 * - init()        – DB megnyitás/létrehozás, tábla init
 * - loadInto()    – DB-ből cookie-k visszatöltése a WebEngine-be
 * - Signalok bekötve: cookieAdded → mentés, cookieRemoved → törlés
 */
class CookieStore : public QObject
{
    Q_OBJECT

public:
    explicit CookieStore(QObject *parent = nullptr);
    ~CookieStore();

    /** DB megnyitás és tábla létrehozás, ha még nincs */
    bool init(const QString &dbPath);

    /** Betölti a mentett cookie-kat a WebEngine cookie store-ba */
    void loadInto(QWebEngineCookieStore *store);

    /** Bekötés: figyeli a cookieAdded / cookieRemoved signalokat */
    void connectTo(QWebEngineCookieStore *store);

public slots:
    void onCookieAdded(const QNetworkCookie &cookie);
    void onCookieRemoved(const QNetworkCookie &cookie);

private:
    void createSchema();
    bool isExpired(const QNetworkCookie &cookie) const;

    QString m_dbPath;
    QString m_connectionName;
};
