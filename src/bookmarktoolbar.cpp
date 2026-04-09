#include "bookmarktoolbar.h"
#include "mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QToolButton>
#include <QIcon>

BookmarkToolBar::BookmarkToolBar(BookmarkStore *store, MainWindow *mainWindow, QWidget *parent)
    : QToolBar("Könyvjelzők", parent)
    , m_store(store)
    , m_mainWindow(mainWindow)
{
    setMovable(false);
    setIconSize(QSize(16, 16));
    setStyleSheet("QToolBar { spacing: 2px; }");
    rebuild();
    connect(m_store, &BookmarkStore::changed, this, &BookmarkToolBar::rebuild);
}

void BookmarkToolBar::rebuild()
{
    clear();

    // Toolbar szintű könyvjelzők (folder == "")
    const QList<Bookmark> items = m_store->inFolder("");

    for (const Bookmark &bm : items) {
        // Rövidített cím max 20 karakter
        QString label = bm.title.isEmpty() ? bm.url : bm.title;
        if (label.length() > 20)
            label = label.left(18) + "…";

        QAction *act = addAction("🔖 " + label);
        act->setToolTip(bm.url);
        const QString url = bm.url;
        connect(act, &QAction::triggered, m_mainWindow, [this, url]() {
            m_mainWindow->navigateTo(QUrl(url));
        });
    }

    // Mappák legördülő menüként
    const QStringList folderList = m_store->folders();
    for (const QString &folder : folderList) {
        QToolButton *btn = new QToolButton(this);
        btn->setText("📁 " + folder + " ▾");
        btn->setPopupMode(QToolButton::InstantPopup);

        QMenu *menu = new QMenu(btn);
        const QList<Bookmark> folderItems = m_store->inFolder(folder);
        for (const Bookmark &bm : folderItems) {
            QString label = bm.title.isEmpty() ? bm.url : bm.title;
            QAction *act = menu->addAction("🔖 " + label);
            act->setToolTip(bm.url);
            const QString url = bm.url;
            connect(act, &QAction::triggered, m_mainWindow, [this, url]() {
                m_mainWindow->navigateTo(QUrl(url));
            });
        }
        btn->setMenu(menu);
        addWidget(btn);
    }

    // Jobb oldalt: "Minden könyvjelző" gomb
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    QAction *allAct = addAction("≡ Kezelés");
    allAct->setToolTip("Könyvjelzőkezelő (Ctrl+Shift+O)");
    connect(allAct, &QAction::triggered, m_mainWindow, &MainWindow::showBookmarkManager);
}
