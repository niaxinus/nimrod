#include "htmlrenderer.h"
#include "csspreprocessor.h"

#include <QRegularExpression>
#include <QTextDocument>

HtmlRenderer::HtmlRenderer(QWidget *parent)
    : QTextBrowser(parent)
    , m_css(new CssPreprocessor(this))
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    connect(this, &QTextBrowser::anchorClicked,
            this, &HtmlRenderer::onAnchorClicked);
}

void HtmlRenderer::renderHtml(const QString &html, const QUrl &baseUrl)
{
    if (!baseUrl.isEmpty()) {
        document()->setBaseUrl(baseUrl);
    }

    // CSS kinyerés és alkalmazás
    QSize viewport = size().isEmpty() ? QSize(1024, 768) : size();
    QString css = m_css->process(html, baseUrl, viewport);
    if (!css.isEmpty()) {
        document()->setDefaultStyleSheet(css);
    }

    setHtml(html);

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
    QTextBrowser::clear();
}

void HtmlRenderer::onAnchorClicked(const QUrl &url)
{
    emit linkActivated(url);
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
