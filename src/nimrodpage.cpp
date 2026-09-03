#include "nimrodpage.h"

#include <QDebug>

NimrodPage::NimrodPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
}

QWebEnginePage *NimrodPage::createWindow(WebWindowType type)
{
    Q_UNUSED(type)

    // Nincs beállított factory → viselkedjünk a régi módon (eldobjuk).
    if (!m_tabFactory) {
        qWarning() << "NimrodPage::createWindow – nincs tab factory, a pop-up eldobva";
        return nullptr;
    }

    // Minden ablaktípust (tab, pop-up, dialog) új lapként kezelünk.
    // A hívó (Chromium) a visszaadott page-re fogja navigálni a kért URL-t.
    return m_tabFactory();
}
