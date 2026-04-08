#pragma once

#include <QMainWindow>
#include <QUrl>

class ConfigManager;
class QLineEdit;
class QToolBar;
class QProgressBar;
class QAction;
class QLabel;
class QWebEngineView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onUrlBarReturn();
    void navigateTo(const QUrl &url);

private:
    void setupUi();
    void setupConnections();

    QWebEngineView *m_view;
    ConfigManager  *m_config;

    QLineEdit    *m_urlBar;
    QAction      *m_backAction;
    QAction      *m_forwardAction;
    QAction      *m_reloadAction;
    QAction      *m_stopAction;
    QProgressBar *m_progressBar;
    QLabel       *m_statusLabel;
};
