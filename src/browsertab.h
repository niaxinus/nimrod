#pragma once

#include <QWidget>
#include <QUrl>

class QWebEngineView;
class QWebEngineProfile;
class QWebEnginePage;
class JsConsole;
class NimrodBridge;
class QWebChannel;

class BrowserTab : public QWidget
{
    Q_OBJECT

public:
    explicit BrowserTab(QWebEngineProfile *profile, NimrodBridge *bridge, QWidget *parent = nullptr);
    ~BrowserTab();

    QWebEngineView *view() const { return m_view; }
    JsConsole      *jsConsole() const { return m_jsConsole; }
    QWebEnginePage *page() const;

    void load(const QUrl &url);
    QUrl url() const;
    QString title() const;

    void back();
    void forward();
    void reload();
    void stop();
    bool canGoBack() const;
    bool canGoForward() const;

    void prepareClose();

    void toggleDevTools();
    void toggleJsConsole();
    void showFindBar();

signals:
    void titleChanged(const QString &title);
    void urlChanged(const QUrl &url);
    void loadStarted();
    void loadProgress(int percent);
    void loadFinished(bool ok);
    void linkHovered(const QString &url);
    void openInNewTab(const QUrl &url);

private:
    void setupConnections();
    void injectAutofillScript();

    QWebEngineView *m_view;
    JsConsole      *m_jsConsole;
    QWebEngineView *m_devToolsView = nullptr;
    QWebChannel    *m_channel      = nullptr;
    QWidget        *m_findBar      = nullptr;
};
