#pragma once
#include <QObject>
#include <QString>
#include "NetworkMessage.hpp"
class QTcpSocket;

namespace Network
{
// Utilisé par le serveur lorsque le client se connecte :
// le client a un NetworkSerializationServer qui tourne
// et le serveur écrit dedans avec le NetworkSerializationSocket
class NetworkSocket : public QObject
{
        Q_OBJECT
    public:
        NetworkSocket(QTcpSocket* sock, QObject* parent);
        NetworkSocket(QString ip, int port, QObject* parent);

        void sendMessage(const NetworkMessage&);

    signals:
        void messageReceived(NetworkMessage);

    private:
        void init();
        QTcpSocket* m_socket{};
};
}
