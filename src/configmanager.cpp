#include "configmanager.h"

#include <QDir>
#include <QStandardPaths>
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_windowSize(1024, 768)
    , m_windowPos(100, 100)
    , m_lastUrl("about:blank")
    , m_theme("light")
    , m_threadCount(0)
{
    ensureConfigDirExists();

    QString configPath = QDir::homePath() + "/.config/nimrod/config.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);
}

ConfigManager::~ConfigManager()
{
    save();
}

void ConfigManager::ensureConfigDirExists()
{
    QString configDir = QDir::homePath() + "/.config/nimrod";
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        qDebug() << "Nimród config könyvtár létrehozva:" << configDir;
    }
}

void ConfigManager::load()
{
    m_windowSize = m_settings->value("window/size", QSize(1024, 768)).toSize();
    m_windowPos  = m_settings->value("window/pos",  QPoint(100, 100)).toPoint();
    m_lastUrl    = m_settings->value("browser/lastUrl", "about:blank").toString();
    m_theme      = m_settings->value("browser/theme", "light").toString();

    // Ha még nincs mentve, egyszer detektálja és elmenti
    if (!m_settings->contains("system/threadCount")) {
        m_threadCount = QThread::idealThreadCount();
        m_settings->setValue("system/threadCount", m_threadCount);
        m_settings->sync();
        qDebug() << "CPU szálak detektálva és elmentve:" << m_threadCount;
    } else {
        m_threadCount = m_settings->value("system/threadCount").toInt();
    }
}

void ConfigManager::save()
{
    m_settings->setValue("window/size",       m_windowSize);
    m_settings->setValue("window/pos",        m_windowPos);
    m_settings->setValue("browser/lastUrl",   m_lastUrl);
    m_settings->setValue("browser/theme",     m_theme);
    // system/threadCount nem íródik felül kilépéskor – csak egyszer detektálódik
    m_settings->sync();
}
