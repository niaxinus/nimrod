#pragma once

#include <QToolBar>
#include "bookmarkstore.h"

class MainWindow;

class BookmarkToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit BookmarkToolBar(BookmarkStore *store, MainWindow *mainWindow, QWidget *parent = nullptr);

    void rebuild();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    BookmarkStore *m_store;
    MainWindow    *m_mainWindow;
};
