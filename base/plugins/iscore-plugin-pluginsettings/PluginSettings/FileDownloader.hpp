#pragma once
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

namespace iscore
{

class FileDownloader : public QObject
{
  Q_OBJECT
public:
  explicit FileDownloader(QUrl url);
  QByteArray downloadedData() const;

signals:
  void downloaded(QByteArray);

private:
  void fileDownloaded(QNetworkReply*);

  QNetworkAccessManager m_mgr;
  QByteArray m_data;
};
}
