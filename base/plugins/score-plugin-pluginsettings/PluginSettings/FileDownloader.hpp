#pragma once
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

namespace score
{

class FileDownloader : public QObject
{
  Q_OBJECT
public:
  explicit FileDownloader(QUrl url);
  QByteArray downloadedData() const;

Q_SIGNALS:
  void downloaded(QByteArray);

private:
  void fileDownloaded(QNetworkReply*);

  QNetworkAccessManager m_mgr;
  QByteArray m_data;
};
}
