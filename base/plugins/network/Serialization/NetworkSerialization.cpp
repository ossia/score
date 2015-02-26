#include "NetworkSerialization.hpp"

/*
 *
 * Côté serveur :
 * On a un NetworkSerializationServer
 *
 * Côté client :
 * On ask une connection.
 *
 */
NetworkSerializationSocket::NetworkSerializationSocket (QString ip, int port, QObject* parent) :
    QObject (parent)
{
    socket = new QTcpSocket (this);

    connect (socket, SIGNAL (connected() ), this, SLOT (connected() ) );
    connect (socket, SIGNAL (disconnected() ), this, SLOT (disconnected() ) );
    connect (socket, SIGNAL (bytesWritten (qint64) ), this, SLOT (bytesWritten (qint64) ) );
    connect (socket, SIGNAL (readyRead() ), this, SLOT (readyRead() ) );

    qDebug() << "connecting...";

    // this is not blocking call
    socket->connectToHost (ip, port);

    // we need to wait...
    if (!socket->waitForConnected (5000) )
    {
        qDebug() << "Error: " << socket->errorString();
    }
}

void NetworkSerializationSocket::connected()
{
    qDebug() << "connected...";

    // Hey server, tell me about you.
    socket->write ("y'a du plop\n");
}

void NetworkSerializationSocket::disconnected()
{
    qDebug() << "disconnected...";
}

void NetworkSerializationSocket::bytesWritten (qint64 bytes)
{
    qDebug() << bytes << " bytes written...";
}

void NetworkSerializationSocket::readyRead()
{
    qDebug() << "reading...";

    // read the data from the socket
    qDebug() << socket->readAll();
}


#include <QNetworkInterface>
NetworkSerializationServer::NetworkSerializationServer (int port, QObject* parent)
{
    m_tcpServer = new QTcpServer (this);

    if (!m_tcpServer->listen() )
    {
        qDebug() << tr ("Unable to start the server: %1.").arg (m_tcpServer->errorString() );

        return;
    }

    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();

    // use the first non-localhost IPv4 address
    for (int i = 0; i < ipAddressesList.size(); ++i)
    {
        if (ipAddressesList.at (i) != QHostAddress::LocalHost &&
                ipAddressesList.at (i).toIPv4Address() )
        {
            ipAddress = ipAddressesList.at (i).toString();
            break;
        }
    }

    // if we did not find one, use IPv4 localhost
    if (ipAddress.isEmpty() )
    {
        ipAddress = QHostAddress (QHostAddress::LocalHost).toString();
    }

    qDebug() << tr ("The server is running on\n\nIP: %1\nport: %2\n\n")
             .arg (ipAddress)
             .arg (m_tcpServer->serverPort() );
}
