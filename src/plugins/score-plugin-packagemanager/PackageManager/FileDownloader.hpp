#pragma once
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

#include <verdigris>

namespace score
{

class FileDownloader : public QObject
{
  W_OBJECT(FileDownloader)
public:
  explicit FileDownloader(QUrl url);
  QByteArray downloadedData() const;

public:
  void downloaded(QByteArray arg_1) W_SIGNAL(downloaded, arg_1);

private:
  void fileDownloaded(QNetworkReply*);

  QNetworkAccessManager m_mgr;
  QByteArray m_data;
};
}
