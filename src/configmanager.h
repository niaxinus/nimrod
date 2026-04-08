#pragma once

#include <QObject>
#include <QString>
#include <QSize>
#include <QPoint>
#include <QSettings>
#include <QThread>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();

    void load();
    void save();

    QSize windowSize() const { return m_windowSize; }
    QPoint windowPos() const { return m_windowPos; }
    QString lastUrl() const { return m_lastUrl; }
    QString theme() const { return m_theme; }
    int threadCount() const { return m_threadCount; }

    void setWindowSize(const QSize &size) { m_windowSize = size; }
    void setWindowPos(const QPoint &pos) { m_windowPos = pos; }
    void setLastUrl(const QString &url) { m_lastUrl = url; }
    void setTheme(const QString &theme) { m_theme = theme; }

private:
    void ensureConfigDirExists();

    QSettings *m_settings = nullptr;
    QSize m_windowSize;
    QPoint m_windowPos;
    QString m_lastUrl;
    QString m_theme;
    int m_threadCount = 0;
};
