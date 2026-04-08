#include "networkmanager.h"

#include <QNetworkRequest>
#include <QFile>
#include <QTextStream>
#include <QDebug>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &NetworkManager::onReplyFinished);
}

void NetworkManager::loadUrl(const QUrl &url)
{
    abort();

    if (url.scheme() == "file") {
        QString filePath = url.toLocalFile();
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            QString html = stream.readAll();
            emit loadStarted(url);
            emit loadFinished(url, html);
        } else {
            emit loadFailed(url, QString("Nem sikerült megnyitni: %1").arg(filePath));
        }
        return;
    }

    if (url.scheme() == "about") {
        emit loadStarted(url);
        emit loadFinished(url, "<html><body><p>about:blank</p></body></html>");
        return;
    }

    emit loadStarted(url);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_currentReply = m_nam->get(request);
    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &NetworkManager::onDownloadProgress);
}

void NetworkManager::abort()
{
    if (m_currentReply && m_currentReply->isRunning()) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
}

void NetworkManager::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (m_currentReply == reply) {
        m_currentReply = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit loadFailed(reply->url(), reply->errorString());
        return;
    }

    QUrl finalUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (finalUrl.isEmpty()) {
        finalUrl = reply->url();
    }

    QByteArray data = reply->readAll();
    QString html = QString::fromUtf8(data);
    emit loadFinished(finalUrl, html);
}

void NetworkManager::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        emit loadProgress(static_cast<int>(received * 100 / total));
    }
}
