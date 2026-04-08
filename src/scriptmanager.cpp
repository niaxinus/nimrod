#include "scriptmanager.h"

#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

ScriptManager::ScriptManager(QObject *parent)
    : QObject(parent)
{}

void ScriptManager::init(QWebEngineProfile *profile, const QString &scriptsDir)
{
    m_profile    = profile;
    m_scriptsDir = scriptsDir;
    ensureDir();
    injectDefaultScripts();
    reload();
}

void ScriptManager::ensureDir()
{
    QDir dir(m_scriptsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        qDebug() << "ScriptManager: userscripts mappa létrehozva:" << m_scriptsDir;

        // README leírófájl a mappában
        QFile readme(m_scriptsDir + "/README.txt");
        if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream s(&readme);
            s << "Nimród userscripts\n"
              << "==================\n"
              << "Ide helyezd a .js fájlokat, amelyeket minden oldalba be szeretnél fecskendezni.\n"
              << "A szkriptek DocumentReady fázisban futnak (DOM kész állapotban).\n"
              << "Újraindítás után töltődnek be.\n\n"
              << "Példa: dark-mode.js\n";
        }
    }
}

void ScriptManager::injectDefaultScripts()
{
    // Nincs kötelező default szkript – csak ha a felhasználó tesz be egyet
}

void ScriptManager::reload()
{
    if (!m_profile) return;

    // Régi userscriptek törlése
    QWebEngineScriptCollection *scripts = m_profile->scripts();
    const QList<QWebEngineScript> all = scripts->toList();
    for (const QWebEngineScript &s : all) {
        if (s.name().startsWith("nimrod_userscript_")) {
            scripts->remove(s);
        }
    }

    // Új szkriptek betöltése
    QDir dir(m_scriptsDir);
    QStringList jsFiles = dir.entryList({"*.js"}, QDir::Files, QDir::Name);
    int loaded = 0;

    for (const QString &fileName : jsFiles) {
        QFile f(m_scriptsDir + "/" + fileName);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "ScriptManager: nem olvasható:" << fileName;
            continue;
        }
        QString code = QTextStream(&f).readAll();

        QWebEngineScript script;
        script.setName("nimrod_userscript_" + fileName);
        script.setSourceCode(code);
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setRunsOnSubFrames(false);
        scripts->insert(script);
        loaded++;
        qDebug() << "ScriptManager: betöltve:" << fileName;
    }

    if (loaded > 0)
        qDebug() << "ScriptManager:" << loaded << "userscript aktív.";
}
