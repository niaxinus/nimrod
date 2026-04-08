#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QMap>
#include <QSize>

/**
 * CssPreprocessor
 *
 * Kinyeri és összefésüli a CSS-t egy HTML dokumentumból:
 *  - <style> blokkok
 *  - <link rel="stylesheet"> külső fájlok
 *  - @import url(...) rekurzív betöltés
 *  - CSS custom properties (--var) feloldás
 *  - Egyszerű calc() kiértékelés
 *  - Media query szűrés (max-width / min-width)
 */
class CssPreprocessor : public QObject
{
    Q_OBJECT

public:
    explicit CssPreprocessor(QObject *parent = nullptr);

    /**
     * Feldolgozza a HTML-t: kinyeri és összefésüli az összes CSS-t.
     * @param html      A nyers HTML tartalom
     * @param baseUrl   Az oldal alap URL-je (külső CSS betöltéshez)
     * @param viewport  Az aktuális viewport mérete (media query-khez)
     * @return          Összefésült, feloldott CSS string
     */
    QString process(const QString &html, const QUrl &baseUrl, const QSize &viewport);

    /** Session szintű CSS cache törlése */
    void clearCache();

private:
    QString extractStyleBlocks(const QString &html) const;
    QStringList extractLinkStylesheets(const QString &html) const;
    QString loadExternalCss(const QUrl &url);
    QString resolveImports(const QString &css, const QUrl &baseUrl, int depth = 0);
    QString resolveCustomProperties(const QString &css) const;
    QString resolveCalc(const QString &css) const;
    QString filterMediaQueries(const QString &css, const QSize &viewport) const;

    double evaluateCalcExpression(const QString &expr) const;

    QMap<QUrl, QString> m_cache;
    static constexpr int MaxImportDepth = 5;
};
