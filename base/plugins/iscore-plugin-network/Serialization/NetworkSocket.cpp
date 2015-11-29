#include <qabstractsocket.h>
#include <qbytearray.h>
#include <qdatastream.h>
#include <qdebug.h>
#include <qiodevice.h>
#include <qtcpsocket.h>

#include "NetworkSocket.hpp"
#include "Serialization/NetworkMessage.hpp"

NetworkSocket::NetworkSocket(QTcpSocket* sock,
                             QObject* parent):
    QObject{parent},
    m_socket{sock}
{
    init();
}

NetworkSocket::NetworkSocket(QString ip,
                             int port,
                             QObject* parent) :
    QObject{parent},
    m_socket{new QTcpSocket{this}}
{
    init();

    m_socket->connectToHost(ip, port);
    if(!m_socket->waitForConnected(5000))
    {
        qDebug() << "Error: " << m_socket->errorString();
    }
}

void NetworkSocket::sendMessage(const NetworkMessage &mess)
{
    QByteArray b;
    QDataStream writer(&b, QIODevice::WriteOnly);
    writer << mess;
    m_socket->write(b);
}

void NetworkSocket::init()
{
    connect(m_socket, &QAbstractSocket::disconnected, this, [] () { qDebug("Disconnected"); });
    connect(m_socket, &QIODevice::readyRead, this, [=] ()
    {
        QByteArray b = m_socket->readAll();

        QDataStream reader(b);
        NetworkMessage m;
        reader >> m;

        emit messageReceived(m);
    });
}
