#include "FileDownloader.hpp"

namespace iscore
{
FileDownloader::FileDownloader(QUrl imageUrl)
{
    connect(&m_mgr, &QNetworkAccessManager::finished,
            this, &FileDownloader::fileDownloaded);

    QNetworkRequest request(imageUrl);
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
