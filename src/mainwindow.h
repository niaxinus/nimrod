#pragma once

#include <QMainWindow>
#include <QUrl>

class ConfigManager;
class CookieStore;
class JsConsole;
class ScriptManager;
class QLineEdit;
class QProgressBar;
class QAction;
class QLabel;
class QWebEngineView;
class QWebEngineProfile;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void navigateTo(const QUrl &url);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onUrlBarReturn();
    void toggleDevTools();
    void toggleJsConsole();

private:
    void setupUi();
    void setupConnections();

    QWebEngineView    *m_view;
    QWebEngineProfile *m_profile;
    QWebEngineView    *m_devToolsView = nullptr;
    ConfigManager     *m_config;
    CookieStore       *m_cookieStore;
    JsConsole         *m_jsConsole;
    ScriptManager     *m_scriptManager;

    QLineEdit    *m_urlBar;
    QAction      *m_backAction;
    QAction      *m_forwardAction;
    QAction      *m_reloadAction;
    QAction      *m_stopAction;
    QAction      *m_devToolsAction;
    QProgressBar *m_progressBar;
    QLabel       *m_statusLabel;
};
