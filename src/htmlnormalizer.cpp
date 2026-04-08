#include "htmlnormalizer.h"

#include <QRegularExpression>
#include <QDebug>

HtmlNormalizer::HtmlNormalizer(QObject *parent)
    : QObject(parent)
{}

// ── Fő belépési pont ────────────────────────────────────────────────────────

QString HtmlNormalizer::normalize(const QString &html) const
{
    QString result = html;

    result = normalizeSemanticTags(result);
    result = normalizeMark(result);
    result = normalizeFigure(result);
    result = normalizeDetails(result);
    result = normalizePicture(result);
    result = normalizeMediaTags(result);
    result = normalizeCanvasSvg(result);
    result = normalizeProgress(result);
    result = normalizeMeter(result);
    result = normalizeDialog(result);
    result = normalizeTime(result);
    result = normalizeFormElements(result);

    return result;
}

// ── Szemantikus blokk tagek → <div> ─────────────────────────────────────────

QString HtmlNormalizer::normalizeSemanticTags(const QString &html) const
{
    // Ezek a tagek mind egyszerűen <div> aliasok lesznek
    const QStringList blockTags = {
        "article", "section", "main", "nav",
        "header", "footer", "aside", "hgroup",
        "dialog" // dialog külön is kezelve, de ide is kell
    };
    // Inline-szerű szemantikus tagek → <span>
    const QStringList inlineTags = {
        "abbr", "bdi", "bdo", "data", "output",
        "ruby", "rt", "rp", "wbr"
    };

    QString result = html;
    for (const QString &tag : blockTags) {
        result = renameTag(result, tag, "div");
    }
    for (const QString &tag : inlineTags) {
        result = renameTag(result, tag, "span");
    }
    return result;
}

// ── <mark> → sárga háttér ────────────────────────────────────────────────────

QString HtmlNormalizer::normalizeMark(const QString &html) const
{
    // <mark ...> → <span style="background-color:#ffff00;color:#000000" ...>
    static QRegularExpression openRe(
        R"(<mark(\s[^>]*)?>)",
        QRegularExpression::CaseInsensitiveOption
    );
    QString result = html;

    QString out;
    out.reserve(result.size());
    int last = 0;
    auto it = openRe.globalMatch(result);
    while (it.hasNext()) {
        auto m = it.next();
        out += result.mid(last, m.capturedStart() - last);
        QString attrs = m.captured(1); // pl. ' class="x"'

        // Ha van inline style, kiegészítjük, egyébként hozzáadjuk
        static QRegularExpression styleRe(R"(style\s*=\s*["']([^"']*)["'])",
                                          QRegularExpression::CaseInsensitiveOption);
        auto sm = styleRe.match(attrs);
        if (sm.hasMatch()) {
            QString newStyle = "background-color:#ffff00;color:#000000;" + sm.captured(1);
            attrs.replace(sm.capturedStart(), sm.capturedLength(),
                          "style=\"" + newStyle + "\"");
        } else {
            attrs += " style=\"background-color:#ffff00;color:#000000\"";
        }
        out += "<span" + attrs + ">";
        last = m.capturedEnd();
    }
    out += result.mid(last);
    result = out;

    result.replace(QRegularExpression(R"(</mark\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</span>");
    return result;
}

// ── <figure> / <figcaption> ──────────────────────────────────────────────────

QString HtmlNormalizer::normalizeFigure(const QString &html) const
{
    QString result = html;
    // <figure> → <div style="display:block;margin:1em 0">
    static QRegularExpression figOpenRe(
        R"(<figure(\s[^>]*)?>)",
        QRegularExpression::CaseInsensitiveOption
    );
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = figOpenRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += "<div" + m.captured(1) + ">";
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }
    result.replace(QRegularExpression(R"(</figure\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</div>");

    // <figcaption> → <p style="font-style:italic;font-size:0.9em;text-align:center">
    static QRegularExpression capOpenRe(
        R"(<figcaption(\s[^>]*)?>)",
        QRegularExpression::CaseInsensitiveOption
    );
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = capOpenRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += "<p style=\"font-style:italic;font-size:0.9em;text-align:center\"" + m.captured(1) + ">";
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }
    result.replace(QRegularExpression(R"(</figcaption\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</p>");
    return result;
}

// ── <details> / <summary> ────────────────────────────────────────────────────

QString HtmlNormalizer::normalizeDetails(const QString &html) const
{
    // <details> → <div style="border:1px solid #ccc;padding:4px;margin:4px 0">
    // <summary> → <p style="font-weight:bold;cursor:default">▶ ...</p>
    QString result = html;

    result.replace(QRegularExpression(R"(<details(\s[^>]*)?>)", QRegularExpression::CaseInsensitiveOption),
                   "<div style=\"border:1px solid #ccc;padding:4px;margin:4px 0\"\\1>");
    result.replace(QRegularExpression(R"(</details\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</div>");

    result.replace(QRegularExpression(R"(<summary(\s[^>]*)?>)", QRegularExpression::CaseInsensitiveOption),
                   "<p style=\"font-weight:bold;margin:0 0 4px 0\"\\1>▶ ");
    result.replace(QRegularExpression(R"(</summary\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</p>");
    return result;
}

// ── <picture> → legjobb <img> ────────────────────────────────────────────────

QString HtmlNormalizer::normalizePicture(const QString &html) const
{
    // <picture>...</picture> → benne lévő <img> megtartása
    static QRegularExpression picRe(
        R"(<picture[^>]*>(.*?)</picture>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    static QRegularExpression imgRe(
        R"(<img[^>]*>)",
        QRegularExpression::CaseInsensitiveOption
    );

    QString result = html;
    QString out;
    out.reserve(result.size());
    int last = 0;
    auto it = picRe.globalMatch(result);
    while (it.hasNext()) {
        auto m = it.next();
        out += result.mid(last, m.capturedStart() - last);

        // Kinyerjük a <img>-et a <picture> belsejéből
        auto imgMatch = imgRe.match(m.captured(1));
        if (imgMatch.hasMatch()) {
            out += imgMatch.captured(0);
        }
        // Ha nincs img, elhagyjuk a teljes picture blokkot
        last = m.capturedEnd();
    }
    out += result.mid(last);
    return out;
}

// ── <audio> / <video> → placeholder ─────────────────────────────────────────

QString HtmlNormalizer::normalizeMediaTags(const QString &html) const
{
    static QRegularExpression audioRe(
        R"(<audio[^>]*>(.*?)</audio>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    static QRegularExpression videoRe(
        R"(<video[^>]*>(.*?)</video>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );

    QString result = html;

    // Hang placeholder
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = audioRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += "<p style=\"border:1px solid #aaa;padding:4px;background:#f9f9f9\">"
                   "🎵 <i>[Hangfájl – JavaScript nélkül nem lejátszható]</i></p>";
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }

    // Videó placeholder
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = videoRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += "<p style=\"border:1px solid #aaa;padding:4px;background:#f5f5f5\">"
                   "🎬 <i>[Videó – JavaScript nélkül nem lejátszható]</i></p>";
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }

    return result;
}

// ── <canvas> / <svg> → placeholder ──────────────────────────────────────────

QString HtmlNormalizer::normalizeCanvasSvg(const QString &html) const
{
    static QRegularExpression canvasRe(
        R"(<canvas[^>]*>.*?</canvas>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    // Önzáró canvas
    static QRegularExpression canvasSelfRe(
        R"(<canvas[^>]*/?\s*>)",
        QRegularExpression::CaseInsensitiveOption
    );

    // SVG-t megpróbáljuk megtartani egyszerű esetekre, komplex → placeholder
    static QRegularExpression svgRe(
        R"(<svg[^>]*>(.*?)</svg>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );

    QString result = html;
    result.replace(canvasRe, "<p style=\"border:1px dashed #ccc;padding:4px;color:#999\">"
                              "<i>[Canvas grafika]</i></p>");
    result.replace(canvasSelfRe, "");

    // SVG: ha rövid és tartalmaz path/rect/circle stb., megtartjuk (Qt nem rendereli, de legalább nem töri)
    // Hosszabb SVG-t levágjuk
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = svgRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            if (m.capturedLength() > 2000) {
                out += "<span style=\"color:#999\"><i>[SVG grafika]</i></span>";
            } else {
                out += m.captured(0); // rövid SVG megtartva
            }
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }

    return result;
}

// ── Form elemek → szöveges placeholder ──────────────────────────────────────

QString HtmlNormalizer::normalizeFormElements(const QString &html) const
{
    QString result = html;

    // <form> → <div>
    result = renameTag(result, "form", "div");
    result = renameTag(result, "fieldset", "div");
    result = renameTag(result, "legend", "p");

    // <label> → <span>
    result = renameTag(result, "label", "span");

    // <button ...>...</button> → <span style="border:...">...</span>
    static QRegularExpression btnRe(
        R"(<button(\s[^>]*)?>)",
        QRegularExpression::CaseInsensitiveOption
    );
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = btnRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += "<span style=\"border:1px solid #999;border-radius:3px;padding:2px 8px;"
                   "background:#eee;display:inline-block\"" + m.captured(1) + ">";
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }
    result.replace(QRegularExpression(R"(</button\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</span>");

    // <textarea>...</textarea> → [Szövegmező]
    static QRegularExpression textareaRe(
        R"(<textarea[^>]*>.*?</textarea>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    result.replace(textareaRe,
                   "<span style=\"border:1px solid #ccc;padding:2px 4px;background:#fff\">"
                   "<i>[Szövegmező]</i></span>");

    // <select>...</select> → [Lenyíló lista]
    static QRegularExpression selectRe(
        R"(<select[^>]*>.*?</select>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    result.replace(selectRe,
                   "<span style=\"border:1px solid #ccc;padding:2px 4px;background:#fff\">"
                   "<i>[Lenyíló lista ▼]</i></span>");

    // <input> tagek típus szerint
    static QRegularExpression inputRe(
        R"(<input(\s[^>]*)?/?>)",
        QRegularExpression::CaseInsensitiveOption
    );
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = inputRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);

            QString attrs = m.captured(1);
            QString type = extractAttr(attrs, "type").toLower();
            QString value = extractAttr(attrs, "value");
            QString placeholder = extractAttr(attrs, "placeholder");

            if (type == "checkbox") {
                out += "<span style=\"font-family:monospace\">☐</span>";
            } else if (type == "radio") {
                out += "<span style=\"font-family:monospace\">○</span>";
            } else if (type == "submit" || type == "button" || type == "reset") {
                QString label = value.isEmpty() ? (type == "reset" ? "Törlés" : "Küldés") : value;
                out += "<span style=\"border:1px solid #999;padding:2px 8px;background:#eee\">"
                       + label.toHtmlEscaped() + "</span>";
            } else if (type == "hidden") {
                // semmi
            } else {
                // text, email, password, number, stb.
                QString display = placeholder.isEmpty() ? (value.isEmpty() ? "…" : value) : placeholder;
                out += "<span style=\"border:1px solid #ccc;padding:2px 4px;background:#fff;"
                       "min-width:100px;display:inline-block\">"
                       + display.toHtmlEscaped() + "</span>";
            }
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }

    return result;
}

// ── <progress> → ASCII bar ───────────────────────────────────────────────────

QString HtmlNormalizer::normalizeProgress(const QString &html) const
{
    static QRegularExpression progressRe(
        R"(<progress(\s[^>]*)?>.*?</progress>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    static QRegularExpression selfClosingRe(
        R"(<progress(\s[^>]*)?/?\s*>)",
        QRegularExpression::CaseInsensitiveOption
    );

    auto processMatch = [this](const QRegularExpressionMatch &m) -> QString {
        QString attrs = m.captured(1);
        int value = extractAttr(attrs, "value").toInt();
        int max   = extractAttr(attrs, "max").toInt();
        if (max <= 0) max = 100;
        return "<span style=\"font-family:monospace\">" + makeProgressBar(value, max) + "</span>";
    };

    QString result = html;
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = progressRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += processMatch(m);
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = selfClosingRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += processMatch(m);
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }
    return result;
}

// ── <meter> → szöveges repr. ─────────────────────────────────────────────────

QString HtmlNormalizer::normalizeMeter(const QString &html) const
{
    static QRegularExpression meterRe(
        R"(<meter(\s[^>]*)?>.*?</meter>)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );

    QString result = html;
    QString out;
    out.reserve(result.size());
    int last = 0;
    auto it = meterRe.globalMatch(result);
    while (it.hasNext()) {
        auto m = it.next();
        out += result.mid(last, m.capturedStart() - last);
        QString attrs = m.captured(1);
        QString value = extractAttr(attrs, "value");
        QString max   = extractAttr(attrs, "max");
        if (max.isEmpty()) max = "1";
        out += "<span style=\"font-family:monospace\">[" + value + "/" + max + "]</span>";
        last = m.capturedEnd();
    }
    out += result.mid(last);
    return out;
}

// ── <dialog> → bordered div ──────────────────────────────────────────────────

QString HtmlNormalizer::normalizeDialog(const QString &html) const
{
    QString result = html;
    result.replace(QRegularExpression(R"(<dialog(\s[^>]*)?>)", QRegularExpression::CaseInsensitiveOption),
                   "<div style=\"border:2px solid #666;padding:12px;margin:8px 0;background:#fff\"\\1>");
    result.replace(QRegularExpression(R"(</dialog\s*>)", QRegularExpression::CaseInsensitiveOption),
                   "</div>");
    return result;
}

// ── <time> → <span> ──────────────────────────────────────────────────────────

QString HtmlNormalizer::normalizeTime(const QString &html) const
{
    return renameTag(html, "time", "span");
}

// ── Segédfüggvények ──────────────────────────────────────────────────────────

QString HtmlNormalizer::renameTag(const QString &html,
                                   const QString &oldTag,
                                   const QString &newTag) const
{
    QString result = html;

    // Nyitó tag: <oldtag ...> → <newtag ...>
    QRegularExpression openRe(
        "<" + oldTag + R"((\s[^>]*)?>)",
        QRegularExpression::CaseInsensitiveOption
    );
    {
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it = openRe.globalMatch(result);
        while (it.hasNext()) {
            auto m = it.next();
            out += result.mid(last, m.capturedStart() - last);
            out += "<" + newTag + m.captured(1) + ">";
            last = m.capturedEnd();
        }
        out += result.mid(last);
        result = out;
    }

    // Záró tag: </oldtag> → </newtag>
    QRegularExpression closeRe(
        "</" + oldTag + R"(\s*>)",
        QRegularExpression::CaseInsensitiveOption
    );
    result.replace(closeRe, "</" + newTag + ">");

    return result;
}

QString HtmlNormalizer::extractAttr(const QString &tag, const QString &attr) const
{
    QRegularExpression re(
        attr + R"(\s*=\s*["']?([^"'\s>]*)["']?)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = re.match(tag);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString HtmlNormalizer::makeProgressBar(int value, int max) const
{
    const int width = 20;
    int filled = (max > 0) ? qBound(0, value * width / max, width) : 0;
    QString bar = "[";
    bar += QString(filled, QChar(0x2588));      // █
    bar += QString(width - filled, QChar(0x2591)); // ░
    bar += QString("] %1/%2").arg(value).arg(max);
    return bar;
}
