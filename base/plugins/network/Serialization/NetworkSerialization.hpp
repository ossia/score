#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

// Utilisé par le serveur lorsque le client se connecte :
// le client a un NetworkSerializationServer qui tourne
// et le serveur écrit dedans avec le NetworkSerializationSocket
class NetworkSerializationSocket : public QObject
{
        Q_OBJECT
    public:
        NetworkSerializationSocket (QString ip, int port, QObject* parent);

        void doConnect();

    signals:

    public slots:
        void connected();
        void disconnected();
        void bytesWritten (qint64 bytes);
        void readyRead();

    private:
        QTcpSocket* socket;

};


class NetworkSerializationServer : public QObject
{
    public:
        NetworkSerializationServer (int port, QObject* parent);

    private:
        QTcpServer* m_tcpServer;
};
