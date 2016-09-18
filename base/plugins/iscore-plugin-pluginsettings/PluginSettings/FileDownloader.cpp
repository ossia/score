#include "FileDownloader.hpp"

namespace iscore
{
FileDownloader::FileDownloader(QUrl imageUrl)
{
    connect(&m_mgr, &QNetworkAccessManager::finished,
            this, &FileDownloader::fileDownloaded);

    QNetworkRequest request(imageUrl);

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    request.setAttribute(QNetworkRequest::SpdyAllowedAttribute, true);
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    m_mgr.get(request);
}

void FileDownloader::fileDownloaded(QNetworkReply* rep)
{
    m_data = rep->readAll();

    rep->deleteLater();
    emit downloaded(m_data);
}

QByteArray FileDownloader::downloadedData() const
{
    return m_data;
}
}
