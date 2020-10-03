// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FileDownloader.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::FileDownloader)
namespace score
{
FileDownloader::FileDownloader(QUrl imageUrl)
{
  connect(
      &m_mgr,
      &QNetworkAccessManager::finished,
      this,
      &FileDownloader::fileDownloaded);

  QNetworkRequest request(imageUrl);

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  // TODO http2
  request.setAttribute(QNetworkRequest::SpdyAllowedAttribute, true);
  request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
  m_mgr.get(request);
}

void FileDownloader::fileDownloaded(QNetworkReply* rep)
{
  m_data = rep->readAll();

  rep->deleteLater();
  downloaded(m_data);
}

QByteArray FileDownloader::downloadedData() const
{
  return m_data;
}
}
