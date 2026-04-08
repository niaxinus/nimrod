#pragma once

#include <QTextBrowser>
#include <QString>
#include <QUrl>

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

private:
    QString extractTitle(const QString &html) const;
};
