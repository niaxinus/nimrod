#include "htmlrenderer.h"
#include "csspreprocessor.h"
#include "htmlnormalizer.h"
#include "cssselectorengine.h"
#include "csslayoutconverter.h"

#include <QRegularExpression>
#include <QTextDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QFile>
#include <QPixmap>
#include <QImage>
#include <QDebug>

HtmlRenderer::HtmlRenderer(QWidget *parent)
    : QTextBrowser(parent)
    , m_css(new CssPreprocessor(this))
    , m_normalizer(new HtmlNormalizer(this))
    , m_selectorEngine(new CssSelectorEngine(this))
    , m_layoutConverter(new CssLayoutConverter(this))
    , m_nam(new QNetworkAccessManager(this))
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    connect(this, &QTextBrowser::anchorClicked,
            this, &HtmlRenderer::onAnchorClicked);
}

void HtmlRenderer::renderHtml(const QString &html, const QUrl &baseUrl)
{
    m_baseUrl = baseUrl;
    m_imageCache.clear();

    if (!baseUrl.isEmpty()) {
        document()->setBaseUrl(baseUrl);
    }

    // JS eltávolítása
    QString cleanHtml = stripJavaScript(html);

    // 1. HTML5 normalizálás (szemantikus tagek → HTML4 ekvivalens)
    cleanHtml = m_normalizer->normalize(cleanHtml);

    // 2. CSS kinyerés és preprocesszálás
    QSize viewport = size().isEmpty() ? QSize(1024, 768) : size();
    QString css = m_css->process(cleanHtml, baseUrl, viewport);

    // 3. CSS Layout konverzió (flex/grid → table)
    cleanHtml = m_layoutConverter->convert(cleanHtml);

    // 4. CSS szelektorok → inline style inject
    if (!css.isEmpty()) {
        cleanHtml = m_selectorEngine->apply(cleanHtml, css);
        document()->setDefaultStyleSheet(css);
    }

    setHtml(cleanHtml);

    QString title = extractTitle(html);
    if (!title.isEmpty()) {
        emit titleFound(title);
    }
}

void HtmlRenderer::renderPlainText(const QString &text)
{
    setPlainText(text);
}

void HtmlRenderer::clear()
{
    m_imageCache.clear();
    QTextBrowser::clear();
}

void HtmlRenderer::onAnchorClicked(const QUrl &url)
{
    emit linkActivated(url);
}

// ── Képbetöltés ─────────────────────────────────────────────────────────────

QVariant HtmlRenderer::loadResource(int type, const QUrl &url)
{
    if (type == QTextDocument::ImageResource) {
        // Relatív URL feloldása
        QUrl resolved = m_baseUrl.isEmpty() ? url : m_baseUrl.resolved(url);

        if (m_imageCache.contains(resolved)) {
            QImage img;
            img.loadFromData(m_imageCache.value(resolved));
            return img;
        }

        QByteArray data = fetchResource(resolved);
        if (!data.isEmpty()) {
            m_imageCache.insert(resolved, data);
            QImage img;
            if (img.loadFromData(data)) {
                return img;
            }
        }
        // Ha nem sikerült: parent kezeli (üres placeholder)
    }
    return QTextBrowser::loadResource(type, url);
}

QByteArray HtmlRenderer::fetchResource(const QUrl &url)
{
    if (url.scheme() == "file") {
        QFile f(url.toLocalFile());
        if (f.open(QIODevice::ReadOnly)) {
            return f.readAll();
        }
        qWarning() << "Kép nem nyitható meg:" << url.toLocalFile();
        return {};
    }

    if (url.scheme() == "http" || url.scheme() == "https") {
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_nam->get(req);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QByteArray data;
        if (reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            qWarning() << "Kép letöltési hiba:" << url.toString() << reply->errorString();
        }
        reply->deleteLater();
        return data;
    }

    return {};
}

// ── JavaScript eltávolítás ───────────────────────────────────────────────────

QString HtmlRenderer::stripJavaScript(const QString &html) const
{
    QString result = html;

    // <script ...>...</script> blokkok eltávolítása
    static QRegularExpression scriptTagRe(
        R"(<script[^>]*>.*?</script>)",
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption
    );
    result.remove(scriptTagRe);

    // Önzáró <script ... /> eltávolítása
    static QRegularExpression scriptSelfRe(
        R"(<script[^>]*/\s*>)",
        QRegularExpression::CaseInsensitiveOption
    );
    result.remove(scriptSelfRe);

    // on* event attribútumok nullázása (onclick, onload, onmouseover, stb.)
    static QRegularExpression onEventRe(
        R"(\bon\w+\s*=\s*(?:"[^"]*"|'[^']*'|[^\s>]*))",
        QRegularExpression::CaseInsensitiveOption
    );
    result.remove(onEventRe);

    // javascript: href/src attribútumok eltávolítása
    static QRegularExpression jsUrlRe(
        R"((href|src|action)\s*=\s*["']javascript:[^"']*["'])",
        QRegularExpression::CaseInsensitiveOption
    );
    result.remove(jsUrlRe);

    // <noscript> blokkok megőrzése (JS nélküli tartalom megjelenik)
    // – nem töröljük

    return result;
}

QString HtmlRenderer::extractTitle(const QString &html) const
{
    static QRegularExpression titleRe(
        "<title[^>]*>(.*?)</title>",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    auto match = titleRe.match(html);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return {};
}
