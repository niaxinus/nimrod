#pragma once

#include <QObject>
#include <QString>

/**
 * HtmlNormalizer
 *
 * HTML5 tageket Qt6 QTextDocument által támogatott HTML4 ekvivalensre konvertál.
 * Minden transzformáció megőrzi az eredeti class/id/style attribútumokat.
 */
class HtmlNormalizer : public QObject
{
    Q_OBJECT

public:
    explicit HtmlNormalizer(QObject *parent = nullptr);

    /**
     * Normalizálja a HTML-t: HTML5 → Qt-kompatibilis HTML4
     * @param html  Nyers HTML tartalom
     * @return      Qt-kompatibilis HTML
     */
    QString normalize(const QString &html) const;

private:
    // Szemantikus blokk tagek → <div> aliasok
    QString normalizeSemanticTags(const QString &html) const;

    // Speciális HTML5 tagek
    QString normalizeMark(const QString &html) const;
    QString normalizeFigure(const QString &html) const;
    QString normalizeDetails(const QString &html) const;
    QString normalizePicture(const QString &html) const;
    QString normalizeMediaTags(const QString &html) const;    // audio, video
    QString normalizeCanvasSvg(const QString &html) const;
    QString normalizeFormElements(const QString &html) const; // input, button, select, textarea
    QString normalizeProgress(const QString &html) const;
    QString normalizeMeter(const QString &html) const;
    QString normalizeDialog(const QString &html) const;
    QString normalizeTime(const QString &html) const;

    // Segédfüggvény: tag átnevezés (nyitó és záró tagek)
    QString renameTag(const QString &html,
                      const QString &oldTag,
                      const QString &newTag) const;

    // Segédfüggvény: egy tag tartalmát kinyeri (első előfordulás)
    QString extractAttr(const QString &tag, const QString &attr) const;

    // Progress bar ASCII generálás
    QString makeProgressBar(int value, int max) const;
};
