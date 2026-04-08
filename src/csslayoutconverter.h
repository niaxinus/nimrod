#pragma once

#include <QObject>
#include <QString>

/**
 * CssLayoutConverter
 *
 * Flexbox és CSS Grid layout → HTML table-alapú layout konverzió.
 * Qt6 QTextDocument nem támogatja a display:flex / display:grid-et,
 * ezért ezeket <table> elemekre fordítjuk le.
 *
 * Támogatott konverziók:
 *  - display:flex; flex-direction:row → <table><tr><td>
 *  - display:flex; flex-direction:column → egymás alatti <div>
 *  - flex-grow:N → arányos width%
 *  - display:grid; grid-template-columns → <table> arányos TD width-del
 *  - gap / grid-gap → cellspacing/padding
 */
class CssLayoutConverter : public QObject
{
    Q_OBJECT

public:
    explicit CssLayoutConverter(QObject *parent = nullptr);

    /**
     * Flex/Grid inline style-okat és div struktúrát konvertál table-alapúra.
     * @param html      Input HTML (már szelektor-injektált inline style-okkal)
     * @return          Módosított HTML, table alapú layout-tal
     */
    QString convert(const QString &html) const;

private:
    // Flex kontainer konverzió
    QString convertFlexContainers(const QString &html) const;

    // Grid kontainer konverzió
    QString convertGridContainers(const QString &html) const;

    // CSS property kinyerése egy inline style stringből
    QString getCssProp(const QString &style, const QString &prop) const;

    // CSS property eltávolítása egy inline style stringből
    QString removeCssProps(const QString &style, const QStringList &props) const;

    // Szorzóból arányos width% számítás
    QString calcWidths(const QStringList &parts) const;
};
