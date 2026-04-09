#include "mainwindow.h"
#include "browsertab.h"
#include "configmanager.h"
#include "cookiestore.h"
#include "credentialstore.h"
#include "nimrodbridge.h"
#include "scriptmanager.h"
#include "bookmarkstore.h"
#include "bookmarktoolbar.h"

#include <QPointer>

#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QMenu>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEnginePage>
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

    // ── BookmarkStore ─────────────────────────────────────────────────────
    m_bookmarkStore = new BookmarkStore(this);
    m_bookmarkStore->init(configDir + "/bookmarks.db");

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
            updateStarButton(url);
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
    updateStarButton(tab->url());
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

    // ── Menüsor ───────────────────────────────────────────────────────────
    QMenuBar *mb = menuBar();

    // File menü
    QMenu *fileMenu = mb->addMenu("&Fájl");
    QAction *actNewTab = fileMenu->addAction("&Új tab");
    actNewTab->setShortcut(QKeySequence("Ctrl+T"));
    connect(actNewTab, &QAction::triggered, this, [this]() {
        newTab(QUrl("https://www.google.com"));
    });

    QAction *actOpen = fileMenu->addAction("&Megnyitás...");
    actOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actOpen, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Fájl megnyitása", {},
            "Weboldalak (*.html *.htm *.xhtml);;Minden fájl (*)");
        if (!path.isEmpty())
            newTab(QUrl::fromLocalFile(path));
    });

    fileMenu->addSeparator();

    QAction *actClose = fileMenu->addAction("Tab &bezárása");
    actClose->setShortcut(QKeySequence("Ctrl+W"));
    connect(actClose, &QAction::triggered, this, [this]() {
        closeTab(m_tabWidget->currentIndex());
    });

    fileMenu->addSeparator();

    QAction *actQuit = fileMenu->addAction("&Kilépés");
    actQuit->setShortcut(QKeySequence("Ctrl+Q"));
    connect(actQuit, &QAction::triggered, this, &MainWindow::close);

    // Edit menü
    QMenu *editMenu = mb->addMenu("&Szerkesztés");
    auto addEditAction = [&](const QString &label, const char *slot, const QKeySequence &sc) {
        QAction *a = editMenu->addAction(label);
        a->setShortcut(sc);
        connect(a, &QAction::triggered, this, [this, slot]() {
            if (auto t = currentTab())
                t->page()->triggerAction(
                    strcmp(slot, "undo")  == 0 ? QWebEnginePage::Undo  :
                    strcmp(slot, "redo")  == 0 ? QWebEnginePage::Redo  :
                    strcmp(slot, "cut")   == 0 ? QWebEnginePage::Cut   :
                    strcmp(slot, "copy")  == 0 ? QWebEnginePage::Copy  :
                    strcmp(slot, "paste") == 0 ? QWebEnginePage::Paste :
                    QWebEnginePage::SelectAll);
        });
    };
    addEditAction("&Visszavonás",   "undo",  QKeySequence("Ctrl+Z"));
    addEditAction("&Újra",          "redo",  QKeySequence("Ctrl+Y"));
    editMenu->addSeparator();
    addEditAction("Ki&vágás",       "cut",   QKeySequence("Ctrl+X"));
    addEditAction("&Másolás",       "copy",  QKeySequence("Ctrl+C"));
    addEditAction("&Beillesztés",   "paste", QKeySequence("Ctrl+V"));
    editMenu->addSeparator();
    QAction *actFind = editMenu->addAction("&Keresés az oldalon...");
    actFind->setShortcut(QKeySequence("Ctrl+F"));
    connect(actFind, &QAction::triggered, this, [this]() {
        if (auto t = currentTab()) t->showFindBar();
    });

    // Help menü – billentyűkombinációk frissítve a könyvjelző shortcutokkal
    QMenu *helpMenu = mb->addMenu("&Súgó");

    QAction *actKeys = helpMenu->addAction("&Billentyűkombinációk");
    connect(actKeys, &QAction::triggered, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Billentyűkombinációk");
        dlg.resize(420, 380);
        auto *layout = new QVBoxLayout(&dlg);
        auto *tb = new QTextBrowser(&dlg);
        tb->setHtml(R"(
<h3>Billentyűkombinációk</h3>
<table cellpadding="6" cellspacing="0" border="1" style="border-collapse:collapse">
<tr><th>Billentyű</th><th>Funkció</th></tr>
<tr><td>Ctrl+T</td><td>Új tab</td></tr>
<tr><td>Ctrl+O</td><td>Fájl megnyitása</td></tr>
<tr><td>Ctrl+W</td><td>Tab bezárása</td></tr>
<tr><td>Ctrl+Q</td><td>Kilépés</td></tr>
<tr><td>Ctrl+Tab</td><td>Következő tab</td></tr>
<tr><td>Ctrl+Shift+Tab</td><td>Előző tab</td></tr>
<tr><td>Ctrl+F</td><td>Keresés az oldalon</td></tr>
<tr><td>Ctrl+J</td><td>JavaScript konzol</td></tr>
<tr><td>F12</td><td>Fejlesztői eszközök</td></tr>
<tr><td>Ctrl+D</td><td>Könyvjelző hozzáadása / eltávolítása</td></tr>
<tr><td>Ctrl+Shift+O</td><td>Könyvjelzőkezelő</td></tr>
<tr><td>Ctrl+Z</td><td>Visszavonás</td></tr>
<tr><td>Ctrl+Y</td><td>Újra</td></tr>
<tr><td>Ctrl+X</td><td>Kivágás</td></tr>
<tr><td>Ctrl+C</td><td>Másolás</td></tr>
<tr><td>Ctrl+V</td><td>Beillesztés</td></tr>
</table>
)");
        layout->addWidget(tb);
        auto *btn = new QPushButton("Bezárás", &dlg);
        connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
        layout->addWidget(btn);
        dlg.exec();
    });

    helpMenu->addSeparator();

    QAction *actAbout = helpMenu->addAction("&Névjegy");
    connect(actAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "Névjegy – Nimród Böngésző",
            "<h2>Nimród Böngésző</h2>"
            "<p><b>⚠ Csak kísérleti projekt – nem így kell böngészőt fejleszteni!</b></p>"
            "<p>Qt6 WebEngine alapú minimális böngésző.</p>"
            "<p>Copyright &copy; 2026 Komka László</p>");
    });

    // ── Könyvjelzők menü (dinamikus) ──────────────────────────────────────
    m_bookmarkMenu = mb->addMenu("&Könyvjelzők");
    setupBookmarkMenu();
    connect(m_bookmarkStore, &BookmarkStore::changed, this, [this]() {
        setupBookmarkMenu();
    });

    // ── Navigációs toolbar ────────────────────────────────────────────────
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

    // ★ Csillag gomb – könyvjelző hozzáadása/eltávolítása
    m_starButton = new QToolButton(this);
    m_starButton->setText("☆");
    m_starButton->setToolTip("Könyvjelző hozzáadása (Ctrl+D)");
    m_starButton->setStyleSheet("QToolButton { font-size: 16px; border: none; padding: 2px 6px; }");
    toolbar->addWidget(m_starButton);
    connect(m_starButton, &QToolButton::clicked, this, &MainWindow::toggleBookmark);

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
    connect(m_urlBar,        &QLineEdit::returnPressed, this, &MainWindow::onUrlBarReturn);
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

    // ── Könyvjelző toolbar (2. sor) ───────────────────────────────────────
    m_bookmarkToolBar = new BookmarkToolBar(m_bookmarkStore, this, this);
    addToolBar(Qt::TopToolBarArea, m_bookmarkToolBar);
    addToolBarBreak(Qt::TopToolBarArea);
}

// ── Könyvjelzők ────────────────────────────────────────────────────────────

void MainWindow::setupBookmarkMenu()
{
    m_bookmarkMenu->clear();

    QAction *actAdd = m_bookmarkMenu->addAction("★ &Könyvjelző hozzáadása");
    actAdd->setShortcut(QKeySequence("Ctrl+D"));
    connect(actAdd, &QAction::triggered, this, &MainWindow::toggleBookmark);

    QAction *actMgr = m_bookmarkMenu->addAction("&Kezelő...");
    actMgr->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(actMgr, &QAction::triggered, this, &MainWindow::showBookmarkManager);

    m_bookmarkMenu->addSeparator();

    // Alap szintű könyvjelzők
    const QList<Bookmark> rootItems = m_bookmarkStore->inFolder("");
    for (const Bookmark &bm : rootItems) {
        QAction *act = m_bookmarkMenu->addAction("🔖 " + (bm.title.isEmpty() ? bm.url : bm.title));
        act->setToolTip(bm.url);
        const QString url = bm.url;
        connect(act, &QAction::triggered, this, [this, url]() {
            navigateTo(QUrl(url));
        });
    }

    // Mappák almenüként
    const QStringList folderList = m_bookmarkStore->folders();
    for (const QString &folder : folderList) {
        QMenu *sub = m_bookmarkMenu->addMenu("📁 " + folder);
        const QList<Bookmark> items = m_bookmarkStore->inFolder(folder);
        for (const Bookmark &bm : items) {
            QAction *act = sub->addAction("🔖 " + (bm.title.isEmpty() ? bm.url : bm.title));
            act->setToolTip(bm.url);
            const QString url = bm.url;
            connect(act, &QAction::triggered, this, [this, url]() {
                navigateTo(QUrl(url));
            });
        }
    }
}

void MainWindow::toggleBookmark()
{
    auto *tab = currentTab();
    if (!tab) return;

    QString url   = tab->url().toString();
    QString title = tab->title();
    if (url.isEmpty() || url == "about:blank") return;

    if (m_bookmarkStore->hasUrl(url)) {
        // Eltávolítás
        Bookmark bm = m_bookmarkStore->byUrl(url);
        m_bookmarkStore->remove(bm.id);
        m_starButton->setText("☆");
        m_starButton->setToolTip("Könyvjelző hozzáadása (Ctrl+D)");
    } else {
        // Mappa kérdése
        QStringList choices;
        choices << "(Könyvjelzők sáv)";
        choices << m_bookmarkStore->folders();
        choices << "[ Új mappa... ]";

        bool ok = false;
        QString chosen = QInputDialog::getItem(this, "Könyvjelző hozzáadása",
            "Mappa:\n" + title + "\n" + url,
            choices, 0, false, &ok);
        if (!ok) return;

        QString folder;
        if (chosen == "[ Új mappa... ]") {
            QString newFolder = QInputDialog::getText(this, "Új mappa", "Mappa neve:", QLineEdit::Normal, "", &ok);
            if (!ok || newFolder.isEmpty()) return;
            folder = newFolder;
        } else if (chosen != "(Könyvjelzők sáv)") {
            folder = chosen;
        }

        m_bookmarkStore->add(title, url, folder);
        m_starButton->setText("★");
        m_starButton->setToolTip("Könyvjelző eltávolítása (Ctrl+D)");
    }
}

void MainWindow::updateStarButton(const QUrl &url)
{
    if (m_bookmarkStore->hasUrl(url.toString())) {
        m_starButton->setText("★");
        m_starButton->setStyleSheet("QToolButton { font-size: 16px; border: none; padding: 2px 6px; color: gold; }");
        m_starButton->setToolTip("Könyvjelző eltávolítása (Ctrl+D)");
    } else {
        m_starButton->setText("☆");
        m_starButton->setStyleSheet("QToolButton { font-size: 16px; border: none; padding: 2px 6px; }");
        m_starButton->setToolTip("Könyvjelző hozzáadása (Ctrl+D)");
    }
}

void MainWindow::showBookmarkManager()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Könyvjelzőkezelő");
    dlg.resize(600, 450);

    auto *layout = new QVBoxLayout(&dlg);

    QTreeWidget *tree = new QTreeWidget(&dlg);
    tree->setHeaderLabels({"Cím", "URL", "Mappa"});
    tree->setColumnWidth(0, 200);
    tree->setColumnWidth(1, 280);

    auto rebuildTree = [&]() {
        tree->clear();
        // Gyökér elem
        QTreeWidgetItem *rootItem = new QTreeWidgetItem(tree, {"(Könyvjelzők sáv)", "", ""});
        rootItem->setData(0, Qt::UserRole, -1);
        for (const Bookmark &bm : m_bookmarkStore->inFolder("")) {
            auto *item = new QTreeWidgetItem(rootItem, {bm.title, bm.url, ""});
            item->setData(0, Qt::UserRole, bm.id);
        }
        rootItem->setExpanded(true);

        for (const QString &folder : m_bookmarkStore->folders()) {
            QTreeWidgetItem *folderItem = new QTreeWidgetItem(tree, {"📁 " + folder, "", folder});
            folderItem->setData(0, Qt::UserRole, -2);
            for (const Bookmark &bm : m_bookmarkStore->inFolder(folder)) {
                auto *item = new QTreeWidgetItem(folderItem, {bm.title, bm.url, folder});
                item->setData(0, Qt::UserRole, bm.id);
            }
            folderItem->setExpanded(true);
        }
    };
    rebuildTree();
    layout->addWidget(tree);

    // Gombok
    auto *btnLayout = new QHBoxLayout;

    auto *btnOpen = new QPushButton("Megnyitás", &dlg);
    auto *btnEdit = new QPushButton("Átnevezés", &dlg);
    auto *btnDel  = new QPushButton("Törlés",    &dlg);
    auto *btnClose = new QPushButton("Bezárás",  &dlg);

    btnLayout->addWidget(btnOpen);
    btnLayout->addWidget(btnEdit);
    btnLayout->addWidget(btnDel);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    connect(btnOpen, &QPushButton::clicked, this, [&]() {
        auto *item = tree->currentItem();
        if (!item) return;
        int id = item->data(0, Qt::UserRole).toInt();
        if (id <= 0) return;
        QString url = item->text(1);
        navigateTo(QUrl(url));
    });

    connect(btnEdit, &QPushButton::clicked, this, [&]() {
        auto *item = tree->currentItem();
        if (!item) return;
        int id = item->data(0, Qt::UserRole).toInt();
        if (id <= 0) return;
        bool ok = false;
        QString newTitle = QInputDialog::getText(&dlg, "Átnevezés", "Új cím:", QLineEdit::Normal, item->text(0), &ok);
        if (!ok || newTitle.isEmpty()) return;
        m_bookmarkStore->update(id, newTitle, item->text(1), item->text(2));
        rebuildTree();
    });

    connect(btnDel, &QPushButton::clicked, this, [&]() {
        auto *item = tree->currentItem();
        if (!item) return;
        int id = item->data(0, Qt::UserRole).toInt();
        if (id <= 0) return;
        auto res = QMessageBox::question(&dlg, "Törlés", "Törlöd a könyvjelzőt?\n" + item->text(0));
        if (res != QMessageBox::Yes) return;
        m_bookmarkStore->remove(id);
        rebuildTree();
    });

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

    // Dupla kattintás = megnyitás
    connect(tree, &QTreeWidget::itemDoubleClicked, this, [&](QTreeWidgetItem *item) {
        int id = item->data(0, Qt::UserRole).toInt();
        if (id <= 0) return;
        navigateTo(QUrl(item->text(1)));
    });

    dlg.exec();
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
    if (key == Qt::Key_D && mod == Qt::ControlModifier) {
        toggleBookmark();
        return;
    }
    if (key == Qt::Key_O && mod == (Qt::ControlModifier | Qt::ShiftModifier)) {
        showBookmarkManager();
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
