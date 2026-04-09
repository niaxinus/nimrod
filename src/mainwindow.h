#pragma once

#include <QMainWindow>
#include <QUrl>

class ConfigManager;
class CookieStore;
class ScriptManager;
class BrowserTab;
class QTabWidget;
class QLineEdit;
class QProgressBar;
class QAction;
class QLabel;
class QWebEngineProfile;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void navigateTo(const QUrl &url);
    BrowserTab *newTab(const QUrl &url = QUrl());

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onUrlBarReturn();
    void closeTab(int index);
    void onTabChanged(int index);

private:
    void setupUi();
    void connectTab(BrowserTab *tab, int index);
    BrowserTab *currentTab() const;

    QTabWidget        *m_tabWidget;
    QWebEngineProfile *m_profile;
    ConfigManager     *m_config;
    CookieStore       *m_cookieStore;
    ScriptManager     *m_scriptManager;

    QLineEdit    *m_urlBar;
    QAction      *m_backAction;
    QAction      *m_forwardAction;
    QAction      *m_reloadAction;
    QAction      *m_stopAction;
    QProgressBar *m_progressBar;
    QLabel       *m_statusLabel;
};
