#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QWebEngineProfile;

/**
 * ScriptManager – userscript kezelő
 *
 * ~/.config/nimrod/userscripts/ mappában lévő .js fájlokat automatikusan
 * befecskendezi az oldalakba QWebEngineScript segítségével.
 *
 * - loadScripts(): beolvassa és regisztrálja az összes .js fájlt
 * - Az injection ideje: DocumentReady (DOM kész, de oldalscriptek előtt)
 * - A mappa nem létezőként hozza létre, ha hiányzik
 */
class ScriptManager : public QObject
{
    Q_OBJECT

public:
    explicit ScriptManager(QObject *parent = nullptr);

    /** Inicializálás: mappa létrehozás + szkriptek betöltése */
    void init(QWebEngineProfile *profile, const QString &scriptsDir);

    /** Újratölti az összes szkriptet (pl. ha a felhasználó módosított egyet) */
    void reload();

    QString scriptsDir() const { return m_scriptsDir; }

private:
    void ensureDir();
    void injectDefaultScripts();

    QWebEngineProfile *m_profile   = nullptr;
    QString            m_scriptsDir;
};
