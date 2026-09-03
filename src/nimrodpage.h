#pragma once

#include <QWebEnginePage>
#include <functional>

/**
 * NimrodPage – QWebEnginePage leszármazott
 *
 * Fő célja a modern, "nagy" oldalakkal (Facebook, Google, stb.) való
 * kompatibilitás:
 *
 *  1. createWindow() felülírás → a window.open(), a target="_blank" linkek,
 *     valamint a Facebook belépési / OAuth / megosztás pop-up ablakai
 *     új tabként nyílnak meg (alapból a Qt ezeket csendben eldobja).
 *
 *  2. A pop-up ablak létrehozását egy "tab factory" callback végzi, amit a
 *     MainWindow állít be – így a NimrodPage nem függ közvetlenül a
 *     böngésző-ablaktól.
 */
class NimrodPage : public QWebEnginePage
{
    Q_OBJECT

public:
    // Új lapot hoz létre és visszaadja annak page-ét (vagy nullptr).
    using TabFactory = std::function<QWebEnginePage *()>;

    explicit NimrodPage(QWebEngineProfile *profile, QObject *parent = nullptr);

    void setTabFactory(TabFactory factory) { m_tabFactory = std::move(factory); }

protected:
    QWebEnginePage *createWindow(WebWindowType type) override;

private:
    TabFactory m_tabFactory;
};
