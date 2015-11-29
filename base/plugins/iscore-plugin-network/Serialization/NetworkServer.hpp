#pragma once
#include <qobject.h>

class QTcpServer;
class QTcpSocket;

class NetworkServer : public QObject
{
        Q_OBJECT
    public:
        NetworkServer(int port, QObject* parent);
        int port() const;

    signals:
        void newSocket(QTcpSocket* sock);

    private:
        QTcpServer* m_tcpServer{};
};

