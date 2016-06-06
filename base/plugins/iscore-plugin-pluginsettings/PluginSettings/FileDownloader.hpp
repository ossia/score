#pragma once
#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

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
