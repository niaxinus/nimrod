#pragma once

#include <QMainWindow>
#include <QUrl>

class ConfigManager;
class CookieStore;
class CredentialStore;
class NimrodBridge;
class ScriptManager;
class BookmarkStore;
class BookmarkToolBar;
class BrowserTab;
class QTabWidget;
class QLineEdit;
class QToolButton;
class QProgressBar;
class QAction;
class QLabel;
class PageDragLabel;
class QWebEngineProfile;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void navigateTo(const QUrl &url);
    BrowserTab *newTab(const QUrl &url = QUrl());
    void showBookmarkManager();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onUrlBarReturn();
    void closeTab(int index);
    void onTabChanged(int index);
    void updateStarButton(const QUrl &url);

private:
    void setupUi();
    void setupBookmarkMenu();
    void connectTab(BrowserTab *tab, int index);
    BrowserTab *currentTab() const;
    void toggleBookmark();

    QTabWidget        *m_tabWidget;
    QWebEngineProfile *m_profile;
    ConfigManager     *m_config;
    CookieStore       *m_cookieStore;
    CredentialStore   *m_credentialStore;
    NimrodBridge      *m_bridge;
    ScriptManager     *m_scriptManager;
    BookmarkStore     *m_bookmarkStore;
    BookmarkToolBar   *m_bookmarkToolBar;

    QLineEdit    *m_urlBar;
    PageDragLabel *m_pageDragLabel = nullptr;
    QToolButton  *m_starButton;
    QAction      *m_backAction;
    QAction      *m_forwardAction;
    QAction      *m_reloadAction;
    QAction      *m_stopAction;
    QProgressBar *m_progressBar;
    QLabel       *m_statusLabel;
    QMenu        *m_bookmarkMenu;
};

