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
      &m_mgr, &QNetworkAccessManager::finished, this, &FileDownloader::fileDownloaded);

  QNetworkRequest req(imageUrl);
  req.setRawHeader("User-Agent", "curl/7.35.0");

  req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
  req.setAttribute(
      QNetworkRequest::RedirectPolicyAttribute,
      QNetworkRequest::SameOriginRedirectPolicy);
  req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

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
