#include "browsertab.h"
#include "jsconsole.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineHistory>
#include <QVBoxLayout>
#include <QMenu>

BrowserTab::BrowserTab(QWebEngineProfile *profile, QWidget *parent)
    : QWidget(parent)
{
    QWebEnginePage *page = new QWebEnginePage(profile, this);
    m_view = new QWebEngineView(this);
    m_view->setPage(page);

    m_jsConsole = new JsConsole(this);
    m_jsConsole->setPage(page);
    m_jsConsole->hide();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_jsConsole);

    // Jobb klikk – egyedi context menu
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = m_view->createStandardContextMenu();
        menu->addSeparator();
        menu->addAction("Megnyitás új tabban", this, [this]() {
            QUrl hoveredUrl = m_view->page()->requestedUrl();
            if (hoveredUrl.isValid())
                emit openInNewTab(hoveredUrl);
        });
        menu->addAction("Oldal forrásának megtekintése", this, [this]() {
            emit openInNewTab(QUrl("view-source:" + m_view->url().toString()));
        });
        menu->exec(m_view->mapToGlobal(pos));
        menu->deleteLater();
    });

    setupConnections();
}

BrowserTab::~BrowserTab()
{
    if (m_devToolsView) {
        m_devToolsView->close();
        m_devToolsView = nullptr;
    }
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

void BrowserTab::setupConnections()
{
    connect(m_view, &QWebEngineView::titleChanged, this, &BrowserTab::titleChanged);
    connect(m_view, &QWebEngineView::urlChanged,   this, &BrowserTab::urlChanged);
    connect(m_view, &QWebEngineView::loadStarted,  this, &BrowserTab::loadStarted);
    connect(m_view, &QWebEngineView::loadProgress, this, &BrowserTab::loadProgress);
    connect(m_view, &QWebEngineView::loadFinished, this, &BrowserTab::loadFinished);
    connect(m_view->page(), &QWebEnginePage::linkHovered, this, &BrowserTab::linkHovered);
}
