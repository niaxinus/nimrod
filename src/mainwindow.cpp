#include "mainwindow.h"
#include "configmanager.h"
#include "cookiestore.h"
#include "jsconsole.h"
#include "scriptmanager.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineCookieStore>
#include <QWebEngineHistory>
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDockWidget>
#include <QStandardPaths>
#include <QDir>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_config(new ConfigManager(this))
{
    m_config->load();

    // ── Persistent profil ──────────────────────────────────────────────────
    m_profile = new QWebEngineProfile("nimrod", this);

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataPath);
    m_profile->setPersistentStoragePath(dataPath);
    m_profile->setCachePath(dataPath + "/cache");
    m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);

    // ── JavaScript és feature beállítások ──────────────────────────────────
    QWebEngineSettings *s = m_profile->settings();
    s->setAttribute(QWebEngineSettings::JavascriptEnabled,                  true);
    s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows,           true);
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard,       true);
    s->setAttribute(QWebEngineSettings::LocalStorageEnabled,                true);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,      true);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls,    true);
    s->setAttribute(QWebEngineSettings::PluginsEnabled,                     true);
    s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled,           true);
    s->setAttribute(QWebEngineSettings::WebGLEnabled,                       true);
    s->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled,         true);
    s->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled,              true);
    s->setAttribute(QWebEngineSettings::AutoLoadImages,                     true);
    s->setAttribute(QWebEngineSettings::DnsPrefetchEnabled,                 true);
    s->setAttribute(QWebEngineSettings::SpatialNavigationEnabled,           false);
    s->setAttribute(QWebEngineSettings::AllowRunningInsecureContent,        false);

    // ── SQLite cookie tár ──────────────────────────────────────────────────
    QString configDir = QDir::homePath() + "/.config/nimrod";
    QDir().mkpath(configDir);

    m_cookieStore = new CookieStore(this);
    if (m_cookieStore->init(configDir + "/cookies.db")) {
        m_cookieStore->loadInto(m_profile->cookieStore());
        m_cookieStore->connectTo(m_profile->cookieStore());
    }

    // ── ScriptManager – userscripts ────────────────────────────────────────
    m_scriptManager = new ScriptManager(this);
    m_scriptManager->init(m_profile, configDir + "/userscripts");

    // ── WebView ────────────────────────────────────────────────────────────
    QWebEnginePage *page = new QWebEnginePage(m_profile, this);
    m_view = new QWebEngineView(this);
    m_view->setPage(page);

    // ── JS konzol dock ─────────────────────────────────────────────────────
    m_jsConsole = new JsConsole(this);
    m_jsConsole->setPage(page);
    addDockWidget(Qt::BottomDockWidgetArea, m_jsConsole);
    m_jsConsole->hide();

    // Jobb klikk menü bővítése
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = m_view->createStandardContextMenu();
        menu->addSeparator();
        menu->addAction("Oldal forrásának megtekintése", this, [this]() {
            QUrl sourceUrl("view-source:" + m_view->url().toString());
            MainWindow *w = new MainWindow();
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->navigateTo(sourceUrl);
            w->show();
        });
        menu->addAction("JavaScript konzol (Ctrl+J)", this, &MainWindow::toggleJsConsole);
        menu->addAction("DevTools (F12)", this, &MainWindow::toggleDevTools);
        menu->exec(m_view->mapToGlobal(pos));
        menu->deleteLater();
    });

    setCentralWidget(m_view);

    setupUi();
    setupConnections();

    resize(m_config->windowSize());
    move(m_config->windowPos());

    QString lastUrl = m_config->lastUrl();
    navigateTo(QUrl(lastUrl.isEmpty() ? "https://www.google.com" : lastUrl));
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    setWindowTitle("Nimród Böngésző");

    QToolBar *toolbar = addToolBar("Navigáció");
    toolbar->setMovable(false);

    m_backAction    = toolbar->addAction("◀");
    m_forwardAction = toolbar->addAction("▶");
    m_reloadAction  = toolbar->addAction("↻");
    m_stopAction    = toolbar->addAction("✕");
    toolbar->addSeparator();

    m_urlBar = new QLineEdit(this);
    m_urlBar->setPlaceholderText("URL vagy fájl elérési út...");
    toolbar->addWidget(m_urlBar);

    toolbar->addSeparator();
    m_devToolsAction = toolbar->addAction("DevTools");
    m_devToolsAction->setToolTip("F12 – Chrome DevTools megnyitása");

    m_statusLabel = new QLabel("Kész.", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(150);
    m_progressBar->setRange(0, 100);
    m_progressBar->hide();

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);

    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);
}

void MainWindow::setupConnections()
{
    connect(m_urlBar, &QLineEdit::returnPressed, this, &MainWindow::onUrlBarReturn);

    connect(m_backAction,    &QAction::triggered, m_view, &QWebEngineView::back);
    connect(m_forwardAction, &QAction::triggered, m_view, &QWebEngineView::forward);
    connect(m_reloadAction,  &QAction::triggered, m_view, &QWebEngineView::reload);
    connect(m_stopAction,    &QAction::triggered, m_view, &QWebEngineView::stop);
    connect(m_devToolsAction,&QAction::triggered, this,   &MainWindow::toggleDevTools);

    connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
        m_urlBar->setText(url.toString());
        m_config->setLastUrl(url.toString());
        m_backAction->setEnabled(m_view->history()->canGoBack());
        m_forwardAction->setEnabled(m_view->history()->canGoForward());
    });

    connect(m_view, &QWebEngineView::titleChanged, this, [this](const QString &title) {
        setWindowTitle(title.isEmpty() ? "Nimród Böngésző" : title + " – Nimród");
    });

    connect(m_view, &QWebEngineView::loadStarted, this, [this]() {
        m_progressBar->setValue(0);
        m_progressBar->show();
        m_statusLabel->setText("Betöltés...");
        m_stopAction->setEnabled(true);
        m_reloadAction->setEnabled(false);
        // JS konzol page frissítése
        m_jsConsole->setPage(m_view->page());
    });

    connect(m_view, &QWebEngineView::loadProgress, this, [this](int p) {
        m_progressBar->setValue(p);
    });

    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_progressBar->hide();
        m_statusLabel->setText(ok ? "Kész." : "Betöltési hiba.");
        m_stopAction->setEnabled(false);
        m_reloadAction->setEnabled(true);
        m_backAction->setEnabled(m_view->history()->canGoBack());
        m_forwardAction->setEnabled(m_view->history()->canGoForward());
    });

    connect(m_view->page(), &QWebEnginePage::linkHovered, this, [this](const QString &url) {
        m_statusLabel->setText(url.isEmpty() ? "Kész." : url);
    });
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F12) {
        toggleDevTools();
        return;
    }
    if (event->key() == Qt::Key_J && event->modifiers() == Qt::ControlModifier) {
        toggleJsConsole();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::toggleDevTools()
{
    if (!m_devToolsView) {
        // Első megnyitás: külön ablak
        m_devToolsView = new QWebEngineView();
        m_devToolsView->setWindowTitle("Nimród – DevTools");
        m_devToolsView->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_devToolsView, &QObject::destroyed, this, [this]() {
            m_devToolsView = nullptr;
        });

        m_view->page()->setDevToolsPage(m_devToolsView->page());
        m_devToolsView->resize(1024, 600);
        m_devToolsView->show();
    } else {
        if (m_devToolsView->isVisible())
            m_devToolsView->hide();
        else
            m_devToolsView->show();
    }
}

void MainWindow::toggleJsConsole()
{
    m_jsConsole->setPage(m_view->page());
    if (m_jsConsole->isVisible())
        m_jsConsole->hide();
    else
        m_jsConsole->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_config->setWindowSize(size());
    m_config->setWindowPos(pos());
    m_config->setLastUrl(m_view->url().toString());
    m_config->save();

    if (m_devToolsView) m_devToolsView->close();
    event->accept();
}

void MainWindow::onUrlBarReturn()
{
    QString text = m_urlBar->text().trimmed();
    if (text.isEmpty()) return;
    navigateTo(QUrl::fromUserInput(text));
}

void MainWindow::navigateTo(const QUrl &url)
{
    if (!url.isValid()) return;
    m_view->load(url);
}
