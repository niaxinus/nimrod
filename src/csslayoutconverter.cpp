#include "csslayoutconverter.h"

#include <QRegularExpression>
#include <QStringList>
#include <QDebug>
#include <cmath>

CssLayoutConverter::CssLayoutConverter(QObject *parent)
    : QObject(parent)
{}

QString CssLayoutConverter::convert(const QString &html) const
{
    QString result = convertFlexContainers(html);
    result = convertGridContainers(result);
    return result;
}

// ── CSS property helpers ─────────────────────────────────────────────────────

QString CssLayoutConverter::getCssProp(const QString &style, const QString &prop) const
{
    QRegularExpression re(
        "(?:^|;)\\s*" + QRegularExpression::escape(prop) + "\\s*:\\s*([^;]+)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = re.match(style);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString CssLayoutConverter::removeCssProps(const QString &style, const QStringList &props) const
{
    QString result = style;
    for (const QString &p : props) {
        QRegularExpression re(
            "(?:^|\\s*;\\s*)" + QRegularExpression::escape(p) + "\\s*:[^;]*(;|$)",
            QRegularExpression::CaseInsensitiveOption
        );
        result.remove(re);
    }
    // Cleanup
    result.replace(QRegularExpression(";{2,}"), ";");
    result.replace(QRegularExpression("^;|;$"), "");
    return result.trimmed();
}

QString CssLayoutConverter::calcWidths(const QStringList &parts) const
{
    // fr egységek összege
    double total = 0;
    for (const QString &p : parts) {
        if (p.endsWith("fr")) total += p.left(p.length()-2).toDouble();
        else total += 1.0;
    }
    if (total <= 0) total = parts.size();

    QStringList widths;
    for (const QString &p : parts) {
        double w = p.endsWith("fr") ? p.left(p.length()-2).toDouble() : 1.0;
        widths << QString::number(qRound(w / total * 100)) + "%";
    }
    return widths.join(',');
}

// ── Flex konverzió ───────────────────────────────────────────────────────────

QString CssLayoutConverter::convertFlexContainers(const QString &html) const
{
    static QRegularExpression flexRe(
        "<(div|section|article|header|footer|nav|main|aside)\\b([^>]*?)style\\s*=\\s*\"([^\"]*)\""
        "([^>]*)>",
        QRegularExpression::CaseInsensitiveOption
    );

    QString result;
    result.reserve(html.size() + html.size() / 4);
    int last = 0;
    auto it = flexRe.globalMatch(html);

    while (it.hasNext()) {
        auto m = it.next();
        result += html.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();

        QString tagName = m.captured(1);
        QString pre     = m.captured(2);
        QString style   = m.captured(3);
        QString post    = m.captured(4);

        if (getCssProp(style, "display").toLower() != "flex") {
            result += m.captured(0);
            continue;
        }

        QString flexDir = getCssProp(style, "flex-direction");
        if (flexDir.isEmpty()) flexDir = "row";

        QString gap = getCssProp(style, "gap");
        if (gap.isEmpty()) gap = getCssProp(style, "grid-gap");
        int spacing = 0;
        if (!gap.isEmpty()) {
            auto gm = QRegularExpression("(\\d+)px").match(gap);
            if (gm.hasMatch()) spacing = gm.captured(1).toInt();
        }

        QStringList removable = {"display","flex-direction","flex-wrap","justify-content",
                                  "align-items","align-content","gap","grid-gap"};
        QString cleanStyle = removeCssProps(style, removable);

        // Belső tartalom (mélység-követéssel)
        int openPos = m.capturedEnd();
        int depth = 1, closePos = openPos;
        static QRegularExpression anyTagRe("<(/?)([a-zA-Z][a-zA-Z0-9]*)(?:\\s[^>]*)?>",
                                            QRegularExpression::DotMatchesEverythingOption);
        auto tagIt = anyTagRe.globalMatch(html, openPos);
        while (tagIt.hasNext() && depth > 0) {
            auto tm = tagIt.next();
            if (tm.captured(2).toLower() == tagName.toLower()) {
                if (tm.captured(1) == "/") { if (--depth == 0) { closePos = tm.capturedEnd(); } }
                else                       { depth++; }
            }
        }
        QString closingTag = "</" + tagName + ">";
        QString inner = html.mid(openPos, closePos - openPos - closingTag.length());
        last = closePos;

        if (flexDir.toLower() == "column") {
            result += "<div style=\"" + cleanStyle + "\">" + inner + "</div>";
        } else {
            // row: gyerekek → <table><tr><td>
            static QRegularExpression childDivRe(
                "<(div|span|p|li|article|section)\\b([^>]*?)(?:style\\s*=\\s*\"([^\"]*)\")?"
                "([^>]*)>",
                QRegularExpression::CaseInsensitiveOption
            );
            result += "<table width=\"100%\" cellspacing=\"" + QString::number(spacing)
                   + "\" style=\"" + cleanStyle + "\"><tr>";

            int innerLast = 0;
            auto cit = childDivRe.globalMatch(inner);
            while (cit.hasNext()) {
                auto cm = cit.next();
                QString cTag   = cm.captured(1);
                QString cStyle = cm.captured(3);
                QString grow   = getCssProp(cStyle, "flex-grow");
                double w = grow.isEmpty() ? 1.0 : grow.toDouble();
                QString cClean = removeCssProps(cStyle, {"flex-grow","flex-shrink","flex-basis"});

                int cOpen = cm.capturedEnd();
                int cDepth = 1, cClose = cOpen;
                auto ctIt = anyTagRe.globalMatch(inner, cOpen);
                while (ctIt.hasNext() && cDepth > 0) {
                    auto ctm = ctIt.next();
                    if (ctm.captured(2).toLower() == cTag.toLower()) {
                        if (ctm.captured(1) == "/") { if (--cDepth == 0) cClose = ctm.capturedEnd(); }
                        else                        { cDepth++; }
                    }
                }
                QString cClosingTag = "</" + cTag + ">";
                QString cInner = inner.mid(cOpen, cClose - cOpen - cClosingTag.length());

                result += "<td style=\"" + cClean + "\">" + cInner + "</td>";
                innerLast = cClose;
            }
            // Ha nem találtunk gyerekeket, az egész innert egy cellába
            if (innerLast == 0) {
                result += "<td>" + inner + "</td>";
            }
            result += "</tr></table>";
        }
    }
    result += html.mid(last);
    return result;
}

// ── Grid konverzió ───────────────────────────────────────────────────────────

QString CssLayoutConverter::convertGridContainers(const QString &html) const
{
    static QRegularExpression gridRe(
        "<(div|section|article)\\b([^>]*?)style\\s*=\\s*\"([^\"]*)\""
        "([^>]*)>",
        QRegularExpression::CaseInsensitiveOption
    );

    QString result;
    result.reserve(html.size() + html.size() / 4);
    int last = 0;
    auto it = gridRe.globalMatch(html);

    while (it.hasNext()) {
        auto m = it.next();
        result += html.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();

        QString tagName = m.captured(1);
        QString style   = m.captured(3);

        if (getCssProp(style, "display").toLower() != "grid") {
            result += m.captured(0);
            continue;
        }

        QString gridCols = getCssProp(style, "grid-template-columns");
        QString gap = getCssProp(style, "gap");
        if (gap.isEmpty()) gap = getCssProp(style, "grid-gap");
        int spacing = 0;
        if (!gap.isEmpty()) {
            auto gm = QRegularExpression("(\\d+)px").match(gap);
            if (gm.hasMatch()) spacing = gm.captured(1).toInt();
        }

        QStringList colParts;
        if (!gridCols.isEmpty()) {
            static QRegularExpression repeatRe("repeat\\s*\\(\\s*(\\d+)\\s*,\\s*([^)]+)\\)");
            auto rm = repeatRe.match(gridCols);
            if (rm.hasMatch()) {
                int cnt = rm.captured(1).toInt();
                QString u = rm.captured(2).trimmed();
                for (int i = 0; i < cnt; ++i) colParts << u;
            } else {
                for (const QString &p : gridCols.split(' ', Qt::SkipEmptyParts))
                    colParts << p.trimmed();
            }
        }

        QStringList removable = {"display","grid-template-columns","grid-template-rows",
                                  "grid-template","grid","gap","grid-gap",
                                  "justify-items","align-items","justify-content","align-content"};
        QString cleanStyle = removeCssProps(style, removable);

        // Belső tartalom
        int openPos = m.capturedEnd();
        int depth = 1, closePos = openPos;
        static QRegularExpression anyTagRe("<(/?)([a-zA-Z][a-zA-Z0-9]*)(?:\\s[^>]*)?>",
                                            QRegularExpression::DotMatchesEverythingOption);
        auto tagIt = anyTagRe.globalMatch(html, openPos);
        while (tagIt.hasNext() && depth > 0) {
            auto tm = tagIt.next();
            if (tm.captured(2).toLower() == tagName.toLower()) {
                if (tm.captured(1) == "/") { if (--depth == 0) closePos = tm.capturedEnd(); }
                else                       { depth++; }
            }
        }
        QString closingTag = "</" + tagName + ">";
        QString inner = html.mid(openPos, closePos - openPos - closingTag.length());
        last = closePos;

        QString widths = calcWidths(colParts);
        QStringList widthList = widths.split(',', Qt::SkipEmptyParts);

        result += "<table width=\"100%\" cellspacing=\"" + QString::number(spacing)
               + "\" style=\"" + cleanStyle + "\"><tr>";

        static QRegularExpression childRe(
            "<(div|span|p|article|section|li)\\b[^>]*>",
            QRegularExpression::CaseInsensitiveOption
        );
        int colIdx = 0;
        auto cit = childRe.globalMatch(inner);
        while (cit.hasNext()) {
            auto cm = cit.next();
            QString cTag = cm.captured(1);
            int cOpen = cm.capturedEnd();
            int cDepth = 1, cClose = cOpen;
            auto ctIt = anyTagRe.globalMatch(inner, cOpen);
            while (ctIt.hasNext() && cDepth > 0) {
                auto ctm = ctIt.next();
                if (ctm.captured(2).toLower() == cTag.toLower()) {
                    if (ctm.captured(1) == "/") { if (--cDepth == 0) cClose = ctm.capturedEnd(); }
                    else                        { cDepth++; }
                }
            }
            QString cClosingTag = "</" + cTag + ">";
            QString cInner = inner.mid(cOpen, cClose - cOpen - cClosingTag.length());

            QString width = (colIdx < widthList.size()) ? widthList[colIdx] : "auto";
            result += "<td width=\"" + width + "\">" + cInner + "</td>";

            if (colParts.size() > 0 && (colIdx + 1) % colParts.size() == 0) {
                result += "</tr><tr>";
            }
            colIdx++;
        }
        result += "</tr></table>";
    }
    result += html.mid(last);
    return result;
}
