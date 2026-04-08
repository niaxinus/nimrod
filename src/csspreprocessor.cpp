#include "csspreprocessor.h"

#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDebug>
#include <cmath>

CssPreprocessor::CssPreprocessor(QObject *parent)
    : QObject(parent)
{}

// ── Fő belépési pont ────────────────────────────────────────────────────────

QString CssPreprocessor::process(const QString &html, const QUrl &baseUrl, const QSize &viewport)
{
    // 1. <style> blokkok
    QString css = extractStyleBlocks(html);

    // 2. <link rel="stylesheet"> külső fájlok
    const QStringList hrefs = extractLinkStylesheets(html);
    for (const QString &href : hrefs) {
        QUrl cssUrl = baseUrl.resolved(QUrl(href));
        QString external = loadExternalCss(cssUrl);
        if (!external.isEmpty()) {
            css += "\n" + external;
        }
    }

    // 3. @import rekurzív feloldás
    css = resolveImports(css, baseUrl);

    // 4. Custom properties (--var) feloldás
    css = resolveCustomProperties(css);

    // 5. calc() kiértékelés
    css = resolveCalc(css);

    // 6. Media query szűrés
    css = filterMediaQueries(css, viewport);

    return css.trimmed();
}

void CssPreprocessor::clearCache()
{
    m_cache.clear();
}

// ── <style> blokkok kinyerése ────────────────────────────────────────────────

QString CssPreprocessor::extractStyleBlocks(const QString &html) const
{
    QString result;
    static QRegularExpression re(
        "<style[^>]*>(.*?)</style>",
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption
    );

    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        auto m = it.next();
        result += m.captured(1) + "\n";
    }
    return result;
}

// ── <link rel="stylesheet"> href-ek kinyerése ───────────────────────────────

QStringList CssPreprocessor::extractLinkStylesheets(const QString &html) const
{
    QStringList hrefs;
    static QRegularExpression re(
        R"(<link[^>]+rel\s*=\s*["']stylesheet["'][^>]*>)",
        QRegularExpression::CaseInsensitiveOption
    );
    static QRegularExpression hrefRe(
        R"(href\s*=\s*["']([^"']+)["'])",
        QRegularExpression::CaseInsensitiveOption
    );

    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        auto m = it.next();
        auto hm = hrefRe.match(m.captured(0));
        if (hm.hasMatch()) {
            hrefs << hm.captured(1);
        }
    }
    return hrefs;
}

// ── Külső CSS betöltés (file:// + HTTP, cache-sel) ──────────────────────────

QString CssPreprocessor::loadExternalCss(const QUrl &url)
{
    if (m_cache.contains(url)) {
        return m_cache.value(url);
    }

    QString content;

    if (url.scheme() == "file") {
        QFile f(url.toLocalFile());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream s(&f);
            content = s.readAll();
        } else {
            qWarning() << "CSS fájl nem nyitható meg:" << url.toLocalFile();
        }
    } else if (url.scheme() == "http" || url.scheme() == "https") {
        QNetworkAccessManager nam;
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = nam.get(req);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            content = QString::fromUtf8(reply->readAll());
        } else {
            qWarning() << "CSS letöltési hiba:" << url.toString() << reply->errorString();
        }
        reply->deleteLater();
    }

    if (!content.isEmpty()) {
        m_cache.insert(url, content);
    }
    return content;
}

// ── @import rekurzív feloldás ────────────────────────────────────────────────

QString CssPreprocessor::resolveImports(const QString &css, const QUrl &baseUrl, int depth)
{
    if (depth >= MaxImportDepth) return css;

    static QRegularExpression importRe(
        R"(@import\s+(?:url\(["']?([^"')]+)["']?\)|["']([^"']+)["'])\s*;)",
        QRegularExpression::CaseInsensitiveOption
    );

    QString result = css;
    int offset = 0;
    auto it = importRe.globalMatch(css);
    int delta = 0;

    while (it.hasNext()) {
        auto m = it.next();
        QString href = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        QUrl importUrl = baseUrl.resolved(QUrl(href));
        QString imported = loadExternalCss(importUrl);
        if (!imported.isEmpty()) {
            imported = resolveImports(imported, importUrl, depth + 1);
        }

        int pos = m.capturedStart() + delta;
        int len = m.capturedLength();
        result.replace(pos, len, imported);
        delta += imported.length() - len;
    }
    return result;
}

// ── CSS custom properties feloldás ──────────────────────────────────────────

QString CssPreprocessor::resolveCustomProperties(const QString &css) const
{
    // 1. Összegyűjtjük az összes --változó: érték definíciót
    QMap<QString, QString> vars;
    static QRegularExpression defRe(R"(--([\w-]+)\s*:\s*([^;}\n]+))");
    auto it = defRe.globalMatch(css);
    while (it.hasNext()) {
        auto m = it.next();
        vars.insert("--" + m.captured(1).trimmed(), m.captured(2).trimmed());
    }

    if (vars.isEmpty()) return css;

    // 2. var(--változó) hivatkozások feloldása (max 3 körös)
    QString result = css;
    static QRegularExpression varRe(R"(var\(\s*(--[\w-]+)\s*(?:,\s*([^)]*))?\))");

    for (int round = 0; round < 3; ++round) {
        QString prev = result;
        QString out;
        out.reserve(result.size());
        int last = 0;
        auto it2 = varRe.globalMatch(result);
        while (it2.hasNext()) {
            auto mv = it2.next();
            out += result.mid(last, mv.capturedStart() - last);
            QString name     = mv.captured(1).trimmed();
            QString fallback = mv.captured(2).trimmed();
            out += vars.value(name, fallback.isEmpty() ? mv.captured(0) : fallback);
            last = mv.capturedEnd();
        }
        out += result.mid(last);
        result = out;
        if (result == prev) break;
    }

    return result;
}

// ── calc() kiértékelés ───────────────────────────────────────────────────────

double CssPreprocessor::evaluateCalcExpression(const QString &expr) const
{
    // Csak px + px, px - px, szám * szám, szám / szám alakok
    QString e = expr.trimmed();

    // Eltávolítjuk az egységeket az egyszerű kifejezések kedvéért
    static QRegularExpression addRe(R"(([\d.]+)px\s*\+\s*([\d.]+)px)");
    static QRegularExpression subRe(R"(([\d.]+)px\s*-\s*([\d.]+)px)");
    static QRegularExpression mulRe(R"(([\d.]+)\s*\*\s*([\d.]+))");
    static QRegularExpression divRe(R"(([\d.]+)\s*\/\s*([\d.]+))");

    auto addM = addRe.match(e);
    if (addM.hasMatch()) return addM.captured(1).toDouble() + addM.captured(2).toDouble();

    auto subM = subRe.match(e);
    if (subM.hasMatch()) return subM.captured(1).toDouble() - subM.captured(2).toDouble();

    auto mulM = mulRe.match(e);
    if (mulM.hasMatch()) return mulM.captured(1).toDouble() * mulM.captured(2).toDouble();

    auto divM = divRe.match(e);
    if (divM.hasMatch()) {
        double d = divM.captured(2).toDouble();
        return (d != 0) ? divM.captured(1).toDouble() / d : 0.0;
    }

    // Egyszerű szám
    static QRegularExpression numRe(R"(^([\d.]+))");
    auto numM = numRe.match(e);
    if (numM.hasMatch()) return numM.captured(1).toDouble();

    return 0.0;
}

QString CssPreprocessor::resolveCalc(const QString &css) const
{
    static QRegularExpression calcRe(R"(calc\(([^)]+)\))");
    QString result;
    result.reserve(css.size());
    int last = 0;
    auto it = calcRe.globalMatch(css);
    while (it.hasNext()) {
        auto m = it.next();
        result += css.mid(last, m.capturedStart() - last);
        double val = evaluateCalcExpression(m.captured(1));
        if (m.captured(1).contains("px")) {
            result += QString::number(static_cast<int>(std::round(val))) + "px";
        } else {
            result += QString::number(val);
        }
        last = m.capturedEnd();
    }
    result += css.mid(last);
    return result;
}

// ── Media query szűrés ───────────────────────────────────────────────────────

QString CssPreprocessor::filterMediaQueries(const QString &css, const QSize &viewport) const
{
    static QRegularExpression mediaRe(
        R"(@media\s+([^{]+)\{((?:[^{}]*|\{[^{}]*\})*)\})",
        QRegularExpression::DotMatchesEverythingOption
    );
    static QRegularExpression maxWidthRe(R"(max-width\s*:\s*([\d.]+)px)");
    static QRegularExpression minWidthRe(R"(min-width\s*:\s*([\d.]+)px)");
    static QRegularExpression maxHeightRe(R"(max-height\s*:\s*([\d.]+)px)");
    static QRegularExpression minHeightRe(R"(min-height\s*:\s*([\d.]+)px)");

    QString result = css;
    int delta = 0;
    auto it = mediaRe.globalMatch(css);

    while (it.hasNext()) {
        auto m = it.next();
        QString condition = m.captured(1).trimmed().toLower();
        QString body      = m.captured(2);

        bool matches = true;

        auto mxw = maxWidthRe.match(condition);
        if (mxw.hasMatch() && viewport.width() > mxw.captured(1).toDouble())
            matches = false;

        auto mnw = minWidthRe.match(condition);
        if (mnw.hasMatch() && viewport.width() < mnw.captured(1).toDouble())
            matches = false;

        auto mxh = maxHeightRe.match(condition);
        if (mxh.hasMatch() && viewport.height() > mxh.captured(1).toDouble())
            matches = false;

        auto mnh = minHeightRe.match(condition);
        if (mnh.hasMatch() && viewport.height() < mnh.captured(1).toDouble())
            matches = false;

        // print media kiszűrése
        if (condition.contains("print")) matches = false;

        int pos = m.capturedStart() + delta;
        int len = m.capturedLength();
        QString replacement = matches ? body : QString();
        result.replace(pos, len, replacement);
        delta += replacement.length() - len;
    }

    return result;
}
