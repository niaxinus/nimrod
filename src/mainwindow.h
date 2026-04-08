#pragma once

#include <QMainWindow>
#include <QUrl>
#include <QStack>

class HtmlRenderer;
class ConfigManager;
class NetworkManager;
class QLineEdit;
class QToolBar;
class QStatusBar;
class QProgressBar;
class QAction;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void navigateTo(const QUrl &url);
    void onUrlBarReturn();
    void onBackClicked();
    void onForwardClicked();
    void onReloadClicked();
    void onStopClicked();
    void onLoadStarted(const QUrl &url);
    void onLoadFinished(const QUrl &finalUrl, const QString &html);
    void onLoadFailed(const QUrl &url, const QString &errorMsg);
    void onLoadProgress(int percent);
    void onTitleFound(const QString &title);
    void onLinkActivated(const QUrl &url);

private:
    void setupUi();
    void setupConnections();
    void updateNavButtons();
    void pushHistory(const QUrl &url);

    HtmlRenderer   *m_renderer;
    ConfigManager  *m_config;
    NetworkManager *m_network;

    QLineEdit    *m_urlBar;
    QAction      *m_backAction;
    QAction      *m_forwardAction;
    QAction      *m_reloadAction;
    QAction      *m_stopAction;
    QProgressBar *m_progressBar;
    QLabel       *m_statusLabel;

    QStack<QUrl> m_backStack;
    QStack<QUrl> m_forwardStack;
    QUrl         m_currentUrl;
    bool         m_loading = false;
};
