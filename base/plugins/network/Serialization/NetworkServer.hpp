#pragma once
#include <QTcpServer>


class NetworkServer : public QObject
{
        Q_OBJECT
    public:
        NetworkServer(int port, QObject* parent);

    signals:
        void newSocket(QTcpSocket* sock);

    public slots:
        void acceptConnection();

    private:
        QTcpServer* m_tcpServer{};
};

