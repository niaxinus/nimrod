#pragma once

#include <QTextBrowser>
#include <QString>
#include <QUrl>
#include <QMap>
#include <QByteArray>
#include <QNetworkAccessManager>

class CssPreprocessor;
class HtmlNormalizer;
class CssSelectorEngine;
class CssLayoutConverter;

class HtmlRenderer : public QTextBrowser
{
    Q_OBJECT

public:
    explicit HtmlRenderer(QWidget *parent = nullptr);

    void renderHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void renderPlainText(const QString &text);
    void clear();

signals:
    void linkActivated(const QUrl &url);
    void titleFound(const QString &title);

private slots:
    void onAnchorClicked(const QUrl &url);

protected:
    // Képbetöltés override – QTextBrowser ezt hívja img src-hez
    QVariant loadResource(int type, const QUrl &url) override;

private:
    QString extractTitle(const QString &html) const;
    QString stripJavaScript(const QString &html) const;
    QByteArray fetchResource(const QUrl &url);

    CssPreprocessor       *m_css;
    HtmlNormalizer        *m_normalizer;
    CssSelectorEngine     *m_selectorEngine;
    CssLayoutConverter    *m_layoutConverter;
    QNetworkAccessManager *m_nam;
    QMap<QUrl, QByteArray> m_imageCache;
    QUrl                   m_baseUrl;
};

