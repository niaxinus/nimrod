#include "mainwindow.h"
#include "browsertab.h"
#include "configmanager.h"
#include "cookiestore.h"
#include "credentialstore.h"
#include "nimrodbridge.h"
#include "scriptmanager.h"

#include <QPointer>

#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineCookieStore>
#include <QTabWidget>
#include <QTabBar>
#include <QAction>
#include <QLineEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QDir>

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

    // ── JavaScript és feature beállítások ─────────────────────────────────
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

    // ── CredentialStore + NimrodBridge ────────────────────────────────────
    m_credentialStore = new CredentialStore(this);
    m_credentialStore->init(configDir + "/credentials.db");
    m_bridge = new NimrodBridge(m_credentialStore, this);

    // ── ScriptManager ──────────────────────────────────────────────────────
    m_scriptManager = new ScriptManager(this);
    m_scriptManager->init(m_profile, configDir + "/userscripts");

    // ── Tab widget ─────────────────────────────────────────────────────────
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    setCentralWidget(m_tabWidget);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabWidget, &QTabWidget::currentChanged,    this, &MainWindow::onTabChanged);

    setupUi();

    resize(m_config->windowSize());
    move(m_config->windowPos());

    // Első tab
    QString lastUrl = m_config->lastUrl();
    newTab(QUrl(lastUrl.isEmpty() ? "https://www.google.com" : lastUrl));
}

MainWindow::~MainWindow() {}

// ── Tab kezelés ────────────────────────────────────────────────────────────

BrowserTab *MainWindow::newTab(const QUrl &url)
{
    BrowserTab *tab = new BrowserTab(m_profile, m_bridge, m_tabWidget);
    int idx = m_tabWidget->addTab(tab, "Új tab");
    m_tabWidget->setCurrentIndex(idx);
    connectTab(tab, idx);

    connect(tab, &BrowserTab::openInNewTab, this, [this](const QUrl &u) {
        newTab(u);
    });

    if (url.isValid() && !url.isEmpty())
        tab->load(url);

    return tab;
}

void MainWindow::connectTab(BrowserTab *tab, int index)
{
    // QPointer: ha a tab törlődik, a pointer null lesz → nincs use-after-free
    QPointer<BrowserTab> safeTab(tab);

    connect(tab, &BrowserTab::titleChanged, this, [this, safeTab](const QString &title) {
        if (!safeTab) return;
        int idx = m_tabWidget->indexOf(safeTab);
        if (idx >= 0) {
            QString label = title.isEmpty() ? "Új tab" : title;
            if (label.length() > 30) label = label.left(28) + "…";
            m_tabWidget->setTabText(idx, label);
        }
        if (m_tabWidget->currentWidget() == safeTab)
            setWindowTitle(title.isEmpty() ? "Nimród Böngésző" : title + " – Nimród");
    });

    connect(tab, &BrowserTab::urlChanged, this, [this, safeTab](const QUrl &url) {
        if (!safeTab) return;
        if (m_tabWidget->currentWidget() == safeTab) {
            m_urlBar->setText(url.toString());
            m_config->setLastUrl(url.toString());
            m_backAction->setEnabled(safeTab->canGoBack());
            m_forwardAction->setEnabled(safeTab->canGoForward());
        }
    });

    connect(tab, &BrowserTab::loadStarted, this, [this, safeTab]() {
        if (!safeTab) return;
        if (m_tabWidget->currentWidget() == safeTab) {
            m_progressBar->setValue(0);
            m_progressBar->show();
            m_statusLabel->setText("Betöltés...");
            m_stopAction->setEnabled(true);
            m_reloadAction->setEnabled(false);
        }
    });

    connect(tab, &BrowserTab::loadProgress, this, [this, safeTab](int p) {
        if (!safeTab) return;
        if (m_tabWidget->currentWidget() == safeTab)
            m_progressBar->setValue(p);
    });

    connect(tab, &BrowserTab::loadFinished, this, [this, safeTab](bool ok) {
        if (!safeTab) return;
        if (m_tabWidget->currentWidget() == safeTab) {
            m_progressBar->hide();
            m_statusLabel->setText(ok ? "Kész." : "Betöltési hiba.");
            m_stopAction->setEnabled(false);
            m_reloadAction->setEnabled(true);
            m_backAction->setEnabled(safeTab->canGoBack());
            m_forwardAction->setEnabled(safeTab->canGoForward());
        }
    });

    connect(tab, &BrowserTab::linkHovered, this, [this, safeTab](const QString &url) {
        if (!safeTab) return;
        if (m_tabWidget->currentWidget() == safeTab)
            m_statusLabel->setText(url.isEmpty() ? "Kész." : url);
    });

    Q_UNUSED(index)
}

void MainWindow::closeTab(int index)
{
    if (m_tabWidget->count() <= 1) return; // legalább 1 tab marad

    BrowserTab *tab = qobject_cast<BrowserTab*>(m_tabWidget->widget(index));
    if (tab) {
        // Explicit disconnect: megakadályozza, hogy async WebEngine signalok
        // a törlés után tüzeljenek és use-after-free-t okozzanak
        disconnect(tab, nullptr, this, nullptr);
        tab->prepareClose();
    }

    m_tabWidget->removeTab(index);
    if (tab)
        tab->deleteLater();
}

void MainWindow::onTabChanged(int index)
{
    BrowserTab *tab = qobject_cast<BrowserTab*>(m_tabWidget->widget(index));
    if (!tab) return;

    m_urlBar->setText(tab->url().toString());
    m_backAction->setEnabled(tab->canGoBack());
    m_forwardAction->setEnabled(tab->canGoForward());
    setWindowTitle(tab->title().isEmpty() ? "Nimród Böngésző" : tab->title() + " – Nimród");
    m_progressBar->hide();
    m_statusLabel->setText("Kész.");
}

BrowserTab *MainWindow::currentTab() const
{
    return qobject_cast<BrowserTab*>(m_tabWidget->currentWidget());
}

// ── Navigáció ──────────────────────────────────────────────────────────────

void MainWindow::navigateTo(const QUrl &url)
{
    if (!url.isValid()) return;
    BrowserTab *tab = currentTab();
    if (tab)
        tab->load(url);
    else
        newTab(url);
}

void MainWindow::onUrlBarReturn()
{
    QString text = m_urlBar->text().trimmed();
    if (text.isEmpty()) return;
    navigateTo(QUrl::fromUserInput(text));
}

// ── UI ─────────────────────────────────────────────────────────────────────

void MainWindow::setupUi()
{
    setWindowTitle("Nimród Böngésző");

    QToolBar *toolbar = addToolBar("Navigáció");
    toolbar->setMovable(false);

    // Új tab gomb a toolbar-on
    QAction *newTabAction = toolbar->addAction("+");
    newTabAction->setToolTip("Új tab (Ctrl+T)");
    connect(newTabAction, &QAction::triggered, this, [this]() {
        newTab(QUrl("https://www.google.com"));
    });
    toolbar->addSeparator();

    m_backAction    = toolbar->addAction("◀");
    m_forwardAction = toolbar->addAction("▶");
    m_reloadAction  = toolbar->addAction("↻");
    m_stopAction    = toolbar->addAction("✕");
    toolbar->addSeparator();

    m_urlBar = new QLineEdit(this);
    m_urlBar->setPlaceholderText("URL vagy keresési kifejezés...");
    toolbar->addWidget(m_urlBar);

    m_statusLabel = new QLabel("Kész.", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(150);
    m_progressBar->setRange(0, 100);
    m_progressBar->hide();

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);

    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);

    // Toolbar gombok → aktív tab
    connect(m_urlBar,       &QLineEdit::returnPressed, this, &MainWindow::onUrlBarReturn);
    connect(m_backAction,    &QAction::triggered, this, [this]() {
        if (auto t = currentTab()) t->back();
    });
    connect(m_forwardAction, &QAction::triggered, this, [this]() {
        if (auto t = currentTab()) t->forward();
    });
    connect(m_reloadAction,  &QAction::triggered, this, [this]() {
        if (auto t = currentTab()) t->reload();
    });
    connect(m_stopAction,    &QAction::triggered, this, [this]() {
        if (auto t = currentTab()) t->stop();
    });
}

// ── Billentyűk ─────────────────────────────────────────────────────────────

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    Qt::KeyboardModifiers mod = event->modifiers();
    int key = event->key();

    if (key == Qt::Key_T && mod == Qt::ControlModifier) {
        newTab(QUrl("https://www.google.com"));
        return;
    }
    if (key == Qt::Key_W && mod == Qt::ControlModifier) {
        closeTab(m_tabWidget->currentIndex());
        return;
    }
    if (key == Qt::Key_Tab && mod == Qt::ControlModifier) {
        int next = (m_tabWidget->currentIndex() + 1) % m_tabWidget->count();
        m_tabWidget->setCurrentIndex(next);
        return;
    }
    if (key == Qt::Key_Tab && mod == (Qt::ControlModifier | Qt::ShiftModifier)) {
        int prev = (m_tabWidget->currentIndex() - 1 + m_tabWidget->count()) % m_tabWidget->count();
        m_tabWidget->setCurrentIndex(prev);
        return;
    }
    if (key == Qt::Key_F12) {
        if (auto t = currentTab()) t->toggleDevTools();
        return;
    }
    if (key == Qt::Key_J && mod == Qt::ControlModifier) {
        if (auto t = currentTab()) t->toggleJsConsole();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

// ── Bezárás ────────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_config->setWindowSize(size());
    m_config->setWindowPos(pos());
    if (auto t = currentTab())
        m_config->setLastUrl(t->url().toString());
    m_config->save();
    event->accept();
}
