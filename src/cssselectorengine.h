#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>

/**
 * CssSelectorEngine
 *
 * CSS szabályokban lévő szelektorokat értelmezi, és a DOM-on végighaladva
 * a megfelelő elemekbe inline style="" attribútumként injektálja a stílust.
 *
 * Támogatott szelektorok:
 *  - tag           pl. p, div, h1
 *  - .class        pl. .highlight
 *  - #id           pl. #header
 *  - tag.class     pl. p.note
 *  - A B           leszármazott
 *  - A > B         közvetlen gyerek
 *  - A, B          többszörös (vesszővel elválasztott)
 *  - [attr]        attribútum létezik
 *  - [attr=val]    attribútum értéke egyezik
 *  - :first-child, :last-child
 */
class CssSelectorEngine : public QObject
{
    Q_OBJECT

public:
    explicit CssSelectorEngine(QObject *parent = nullptr);

    /**
     * Alkalmazza a CSS szabályokat a HTML-re.
     * @param html      Input HTML (lehetőleg már normalizált)
     * @param css       Összefésült CSS string (a CssPreprocessor kimenete)
     * @return          Módosított HTML, ahol a CSS stílusok inline style-ban vannak
     */
    QString apply(const QString &html, const QString &css) const;

private:
    struct CssRule {
        QStringList selectors; // több szelektor (vesszővel elválasztva)
        QString declarations;  // pl. "color:red;font-size:14px"
        int specificity = 0;
    };

    // CSS parse
    QVector<CssRule> parseRules(const QString &css) const;
    int calcSpecificity(const QString &selector) const;

    // HTML DOM módosítás (regex alapú, könnyűsúlyú)
    QString injectStyles(const QString &html,
                         const QVector<CssRule> &rules) const;

    // Szelektor illesztés egy elemre
    bool matchesSelector(const QString &selector,
                         const QString &tag,
                         const QString &id,
                         const QString &classAttr,
                         const QString &allAttrs,
                         bool isFirstChild,
                         bool isLastChild) const;

    // Inline style összefésülés (meglévő + új, specificity-alapú)
    QString mergeStyles(const QString &existing, const QString &newStyle) const;

    // Attribútum kinyerése egy tag HTML-jéből
    QString getAttr(const QString &tagHtml, const QString &attr) const;
};
