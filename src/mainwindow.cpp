#include "mainwindow.h"
#include "htmlrenderer.h"
#include "configmanager.h"
#include "networkmanager.h"

#include <QLineEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QAction>
#include <QLabel>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_config(new ConfigManager(this))
    , m_network(new NetworkManager(this))
{
    m_config->load();

    m_renderer = new HtmlRenderer(this);
    setCentralWidget(m_renderer);

    setupUi();
    setupConnections();

    // Ablak állapot visszaállítása
    resize(m_config->windowSize());
    move(m_config->windowPos());

    // Utolsó URL betöltése
    QString lastUrl = m_config->lastUrl();
    navigateTo(QUrl(lastUrl.isEmpty() ? "about:blank" : lastUrl));
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    setWindowTitle("Nimród Böngésző");

    // Toolbar
    QToolBar *toolbar = addToolBar("Navigáció");
    toolbar->setMovable(false);

    m_backAction   = toolbar->addAction("◀ Vissza");
    m_forwardAction = toolbar->addAction("▶ Előre");
    m_reloadAction = toolbar->addAction("↻ Frissít");
    m_stopAction   = toolbar->addAction("✕ Stop");
    toolbar->addSeparator();

    m_urlBar = new QLineEdit(this);
    m_urlBar->setPlaceholderText("Írj be egy URL-t vagy fájl elérési utat...");
    toolbar->addWidget(m_urlBar);

    // Statusbar
    m_statusLabel = new QLabel(this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(150);
    m_progressBar->setRange(0, 100);
    m_progressBar->hide();

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);

    updateNavButtons();
}

void MainWindow::setupConnections()
{
    connect(m_urlBar, &QLineEdit::returnPressed, this, &MainWindow::onUrlBarReturn);
    connect(m_backAction,    &QAction::triggered, this, &MainWindow::onBackClicked);
    connect(m_forwardAction, &QAction::triggered, this, &MainWindow::onForwardClicked);
    connect(m_reloadAction,  &QAction::triggered, this, &MainWindow::onReloadClicked);
    connect(m_stopAction,    &QAction::triggered, this, &MainWindow::onStopClicked);

    connect(m_network, &NetworkManager::loadStarted,  this, &MainWindow::onLoadStarted);
    connect(m_network, &NetworkManager::loadFinished, this, &MainWindow::onLoadFinished);
    connect(m_network, &NetworkManager::loadFailed,   this, &MainWindow::onLoadFailed);
    connect(m_network, &NetworkManager::loadProgress, this, &MainWindow::onLoadProgress);

    connect(m_renderer, &HtmlRenderer::linkActivated, this, &MainWindow::onLinkActivated);
    connect(m_renderer, &HtmlRenderer::titleFound,    this, &MainWindow::onTitleFound);

    // Link hover: mutasd az URL-t alul
    connect(m_renderer, &QTextBrowser::highlighted, this, [this](const QUrl &url) {
        if (url.isEmpty())
            m_statusLabel->setText("Kész.");
        else
            m_statusLabel->setText(url.toString());
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_config->setWindowSize(size());
    m_config->setWindowPos(pos());
    m_config->setLastUrl(m_currentUrl.toString());
    m_config->save();
    event->accept();
}

void MainWindow::navigateTo(const QUrl &url)
{
    QUrl resolved = url;
    if (resolved.scheme().isEmpty()) {
        // Ha nincs scheme, próbáljuk fájlként, aztán http-ként
        QString s = url.toString();
        if (s.startsWith("/") || s.startsWith("./") || s.startsWith("../")) {
            resolved = QUrl::fromLocalFile(s);
        } else if (s.contains('.') && !s.contains(' ')) {
            resolved = QUrl("http://" + s);
        } else {
            resolved = QUrl("about:blank");
        }
    }

    pushHistory(resolved);
    m_urlBar->setText(resolved.toString());
    m_network->loadUrl(resolved);
}

void MainWindow::onUrlBarReturn()
{
    QString text = m_urlBar->text().trimmed();
    if (text.isEmpty()) return;
    navigateTo(QUrl(text));
}

void MainWindow::onBackClicked()
{
    if (m_backStack.size() < 2) return;
    m_forwardStack.push(m_backStack.pop()); // current
    QUrl prev = m_backStack.top();
    m_currentUrl = prev;
    m_urlBar->setText(prev.toString());
    m_network->loadUrl(prev);
    updateNavButtons();
}

void MainWindow::onForwardClicked()
{
    if (m_forwardStack.isEmpty()) return;
    QUrl next = m_forwardStack.pop();
    m_backStack.push(next);
    m_currentUrl = next;
    m_urlBar->setText(next.toString());
    m_network->loadUrl(next);
    updateNavButtons();
}

void MainWindow::onReloadClicked()
{
    if (!m_currentUrl.isEmpty()) {
        m_network->loadUrl(m_currentUrl);
    }
}

void MainWindow::onStopClicked()
{
    m_network->abort();
    m_loading = false;
    m_progressBar->hide();
    m_statusLabel->setText("Megállítva.");
    updateNavButtons();
}

void MainWindow::onLoadStarted(const QUrl &url)
{
    m_loading = true;
    m_statusLabel->setText("Betöltés: " + url.toString());
    m_progressBar->setValue(0);
    m_progressBar->show();
    updateNavButtons();
}

void MainWindow::onLoadFinished(const QUrl &finalUrl, const QString &html)
{
    m_loading = false;
    m_currentUrl = finalUrl;
    m_urlBar->setText(finalUrl.toString());
    m_renderer->renderHtml(html, finalUrl);
    m_statusLabel->setText("Kész.");
    m_progressBar->hide();

    m_config->setLastUrl(finalUrl.toString());
    updateNavButtons();
}

void MainWindow::onLoadFailed(const QUrl &url, const QString &errorMsg)
{
    m_loading = false;
    m_progressBar->hide();
    m_statusLabel->setText("Hiba: " + errorMsg);

    QString errorHtml = QString(
        "<html><body>"
        "<h2>Nem sikerült betölteni az oldalt</h2>"
        "<p><b>URL:</b> %1</p>"
        "<p><b>Hiba:</b> %2</p>"
        "</body></html>"
    ).arg(url.toString().toHtmlEscaped(), errorMsg.toHtmlEscaped());
    m_renderer->renderHtml(errorHtml);
    updateNavButtons();
}

void MainWindow::onLoadProgress(int percent)
{
    m_progressBar->setValue(percent);
}

void MainWindow::onTitleFound(const QString &title)
{
    setWindowTitle(title + " – Nimród Böngésző");
}

void MainWindow::onLinkActivated(const QUrl &url)
{
    navigateTo(url);
}

void MainWindow::updateNavButtons()
{
    m_backAction->setEnabled(m_backStack.size() >= 2);
    m_forwardAction->setEnabled(!m_forwardStack.isEmpty());
    m_reloadAction->setEnabled(!m_loading && !m_currentUrl.isEmpty());
    m_stopAction->setEnabled(m_loading);
}

void MainWindow::pushHistory(const QUrl &url)
{
    if (!m_backStack.isEmpty() && m_backStack.top() == url) return;
    m_backStack.push(url);
    m_forwardStack.clear();
    m_currentUrl = url;
    updateNavButtons();
}
