#include "browsertab.h"
#include "jsconsole.h"
#include "nimrodbridge.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineHistory>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineFullScreenRequest>
#include <QWebChannel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QWebEngineContextMenuRequest>
#include <QMenu>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QWebEnginePermission>
#endif

BrowserTab::BrowserTab(QWebEngineProfile *profile, NimrodBridge *bridge, QWidget *parent)
    : QWidget(parent)
{
    m_page = new NimrodPage(profile, this);
    QWebEnginePage *page = m_page;
    m_view = new QWebEngineView(this);
    m_view->setPage(page);

    // ── QWebChannel – bridge JS ↔ C++ ─────────────────────────────────────
    m_channel = new QWebChannel(this);
    m_channel->registerObject("nimrodBridge", bridge);
    page->setWebChannel(m_channel);

    // Autofill script injektálása
    injectAutofillScript();

    m_jsConsole = new JsConsole(this);
    m_jsConsole->setPage(page);
    m_jsConsole->hide();

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_view, 1);
    m_layout->addWidget(m_jsConsole);

    // Jobb klikk – egyedi context menu
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = m_view->createStandardContextMenu();

        // Link URL a jobb klikk pozíciójánál (Qt 6.2+)
        QUrl linkUrl;
        if (auto *req = m_view->lastContextMenuRequest())
            linkUrl = req->linkUrl();

        menu->addSeparator();

        QAction *openNewTabAct = menu->addAction("Megnyitás új tabban");
        if (linkUrl.isValid()) {
            connect(openNewTabAct, &QAction::triggered, this, [this, linkUrl]() {
                emit openInNewTab(linkUrl);
            });
        } else {
            openNewTabAct->setEnabled(false);
        }

        menu->addAction("Oldal forrásának megtekintése", this, [this]() {
            emit openInNewTab(QUrl("view-source:" + m_view->url().toString()));
        });
        menu->exec(m_view->mapToGlobal(pos));
        menu->deleteLater();
    });

    setupConnections();
    setupPermissionHandling();
}

BrowserTab::~BrowserTab()
{
    if (m_devToolsView) {
        m_devToolsView->close();
        m_devToolsView = nullptr;
    }
    // Ha teljes képernyős videó közben zárjuk be a lapot, a m_view egy külön
    // top-level ablak (nincs szülője) → kézzel kell felszabadítani.
    if (m_isFullScreen && m_view) {
        m_view->setParent(nullptr);
        m_view->deleteLater();
    }
}

void BrowserTab::setTabFactory(NimrodPage::TabFactory factory)
{
    if (m_page)
        m_page->setTabFactory(std::move(factory));
}

void BrowserTab::load(const QUrl &url)
{
    m_view->load(url);
}

QUrl BrowserTab::url() const
{
    return m_view->url();
}

QString BrowserTab::title() const
{
    QString t = m_view->title();
    return t.isEmpty() ? m_view->url().host() : t;
}

void BrowserTab::back()    { m_view->back(); }
void BrowserTab::forward() { m_view->forward(); }
void BrowserTab::reload()  { m_view->reload(); }
void BrowserTab::stop()    { m_view->stop(); }
bool BrowserTab::canGoBack()    const { return m_view->history()->canGoBack(); }
bool BrowserTab::canGoForward() const { return m_view->history()->canGoForward(); }

void BrowserTab::prepareClose()
{
    // Leállítja a betöltést
    m_view->stop();
    // Leválasztja az összes signal kapcsolatot a page-ről és a view-ról
    // hogy az async WebEngine callbackek ne tüzeljenek törlés után
    disconnect(m_view, nullptr, this, nullptr);
    if (m_view->page())
        disconnect(m_view->page(), nullptr, this, nullptr);
    // Üres lapra navigál → megszakítja a hálózati kéréseket
    m_view->setPage(nullptr);
}

void BrowserTab::toggleDevTools()
{
    if (!m_devToolsView) {
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
        m_devToolsView->isVisible() ? m_devToolsView->hide() : m_devToolsView->show();
    }
}

void BrowserTab::toggleJsConsole()
{
    m_jsConsole->setPage(m_view->page());
    m_jsConsole->isVisible() ? m_jsConsole->hide() : m_jsConsole->show();
}

void BrowserTab::injectAutofillScript()
{
    // 1) qwebchannel.js – Qt WebEngine beépített QRC forrásból
    QWebEngineScript wcScript;
    wcScript.setName("nimrod_qwebchannel");
    wcScript.setSourceUrl(QUrl("qrc:///qtwebchannel/qwebchannel.js"));
    wcScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    wcScript.setWorldId(QWebEngineScript::MainWorld);
    wcScript.setRunsOnSubFrames(false);
    m_view->page()->scripts().insert(wcScript);

    // 2) autofill.js – resources/autofill.js beolvasása
    QString autofillPath = QCoreApplication::applicationDirPath() + "/../resources/autofill.js";
    QFile f(autofillPath);
    if (!f.exists()) {
        // Próbáljuk a forrásmappából is
        autofillPath = QString(NIMROD_RESOURCES_DIR) + "/autofill.js";
        f.setFileName(autofillPath);
    }

    QString autofillCode;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        autofillCode = QTextStream(&f).readAll();
    } else {
        qWarning() << "BrowserTab: autofill.js nem található:" << autofillPath;
        return;
    }

    QWebEngineScript afScript;
    afScript.setName("nimrod_autofill");
    afScript.setSourceCode(autofillCode);
    afScript.setInjectionPoint(QWebEngineScript::DocumentReady);
    afScript.setWorldId(QWebEngineScript::MainWorld);
    afScript.setRunsOnSubFrames(false);
    m_view->page()->scripts().insert(afScript);
}

void BrowserTab::setupConnections()
{
    connect(m_view, &QWebEngineView::titleChanged, this, &BrowserTab::titleChanged);
    connect(m_view, &QWebEngineView::urlChanged,   this, &BrowserTab::urlChanged);
    connect(m_view, &QWebEngineView::loadStarted,  this, &BrowserTab::loadStarted);
    connect(m_view, &QWebEngineView::loadProgress, this, &BrowserTab::loadProgress);
    connect(m_view, &QWebEngineView::loadFinished, this, &BrowserTab::loadFinished);
    connect(m_view->page(), &QWebEnginePage::linkHovered, this, &BrowserTab::linkHovered);

    // ── Teljes képernyő kérés (Facebook / YouTube videó, Reels) ───────────
    connect(m_view->page(), &QWebEnginePage::fullScreenRequested, this,
            [this](QWebEngineFullScreenRequest request) {
        request.accept();
        if (request.toggleOn())
            enterFullScreen();
        else
            exitFullScreen();
    });
}

// ── Teljes képernyő: a webnézetet külön top-level ablakká tesszük ──────────
void BrowserTab::enterFullScreen()
{
    if (m_isFullScreen) return;
    m_isFullScreen = true;
    m_layout->removeWidget(m_view);
    m_view->setParent(nullptr);
    m_view->setWindowFlags(Qt::Window);
    m_view->showFullScreen();
    m_view->setFocus();
}

void BrowserTab::exitFullScreen()
{
    if (!m_isFullScreen) return;
    m_isFullScreen = false;
    m_view->setWindowFlags(Qt::Widget);
    m_layout->insertWidget(0, m_view, 1);
    m_view->showNormal();
    m_view->setFocus();
}

// ── Jogosultság-kérések (értesítés, kamera, mikrofon, helyzet) ─────────────
// Facebook induláskor értesítés-jogot kér; a többit alapból elutasítjuk,
// hogy a JS Promise ne "lógjon be" válaszra várva.
void BrowserTab::setupPermissionHandling()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(m_page, &QWebEnginePage::permissionRequested, this,
            [](QWebEnginePermission permission) {
        switch (permission.permissionType()) {
        case QWebEnginePermission::PermissionType::Notifications:
        case QWebEnginePermission::PermissionType::ClipboardReadWrite:
            permission.grant();
            break;
        default:
            permission.deny();
            break;
        }
    });
#else
    connect(m_page, &QWebEnginePage::featurePermissionRequested, this,
            [this](const QUrl &origin, QWebEnginePage::Feature feature) {
        if (feature == QWebEnginePage::Notifications)
            m_page->setFeaturePermission(origin, feature,
                                        QWebEnginePage::PermissionGrantedByUser);
        else
            m_page->setFeaturePermission(origin, feature,
                                        QWebEnginePage::PermissionDeniedByUser);
    });
#endif
}

QWebEnginePage *BrowserTab::page() const
{
    return m_view->page();
}

void BrowserTab::showFindBar()
{
    if (!m_findBar) {
        // Kereső sáv létrehozása a tab aljára
        auto *layout = qobject_cast<QVBoxLayout*>(this->layout());
        if (!layout) return;

        m_findBar = new QWidget(this);
        auto *hbox = new QHBoxLayout(m_findBar);
        hbox->setContentsMargins(4, 2, 4, 2);

        auto *label = new QLabel("Keresés:", m_findBar);
        auto *input = new QLineEdit(m_findBar);
        input->setPlaceholderText("Keresési kifejezés...");
        auto *btnNext = new QPushButton("▼", m_findBar);
        auto *btnPrev = new QPushButton("▲", m_findBar);
        auto *btnClose = new QPushButton("✕", m_findBar);
        btnNext->setMaximumWidth(30);
        btnPrev->setMaximumWidth(30);
        btnClose->setMaximumWidth(30);

        hbox->addWidget(label);
        hbox->addWidget(input);
        hbox->addWidget(btnPrev);
        hbox->addWidget(btnNext);
        hbox->addWidget(btnClose);
        layout->addWidget(m_findBar);

        connect(input, &QLineEdit::textChanged, this, [this, input]() {
            m_view->findText(input->text());
        });
        connect(input, &QLineEdit::returnPressed, this, [this, input]() {
            m_view->findText(input->text());
        });
        connect(btnNext, &QPushButton::clicked, this, [this, input]() {
            m_view->findText(input->text());
        });
        connect(btnPrev, &QPushButton::clicked, this, [this, input]() {
            m_view->findText(input->text(), QWebEnginePage::FindBackward);
        });
        connect(btnClose, &QPushButton::clicked, this, [this]() {
            m_view->findText({}); // töröl
            m_findBar->hide();
        });
    }

    m_findBar->show();
    // Focus az input mezőre
    if (auto *input = m_findBar->findChild<QLineEdit*>())
        input->setFocus();
}
