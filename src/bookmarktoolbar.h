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

private:
    BookmarkStore *m_store;
    MainWindow    *m_mainWindow;
};
