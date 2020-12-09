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

  QNetworkRequest req(imageUrl);
  req.setRawHeader("User-Agent", "curl/7.35.0");

  req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  req.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
  req.setAttribute(QNetworkRequest::SpdyAllowedAttribute, true);
#else
  req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#endif

  m_mgr.get(req);
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
