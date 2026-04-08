#pragma once

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    void loadUrl(const QUrl &url);
    void abort();

signals:
    void loadStarted(const QUrl &url);
    void loadFinished(const QUrl &finalUrl, const QString &html);
    void loadFailed(const QUrl &url, const QString &errorMsg);
    void loadProgress(int percent);

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onDownloadProgress(qint64 received, qint64 total);

private:
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_currentReply = nullptr;
};
