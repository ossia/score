#include "NetworkSerialization.hpp"

NetworkSerializationSocket::NetworkSerializationSocket(QTcpSocket* sock,
                                                       QObject* parent):
    QObject{parent},
    m_socket{sock}
{
    init();
}

NetworkSerializationSocket::NetworkSerializationSocket(QString ip,
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

void NetworkSerializationSocket::sendMessage(NetworkMessage mess)
{
    QByteArray b;
    QDataStream writer(&b, QIODevice::WriteOnly);
    writer << mess;
    m_socket->write(b);
}

void NetworkSerializationSocket::init()
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




#include <QNetworkInterface>
NetworkSerializationServer::NetworkSerializationServer(int port, QObject* parent)
{
    m_tcpServer = new QTcpServer(this);

    if(!m_tcpServer->listen(QHostAddress::Any, port))
    {
        qDebug() << tr("Unable to start the server: %1.").arg(m_tcpServer->errorString());

        return;
    }

    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();

    // use the first non-localhost IPv4 address
    for(int i = 0; i < ipAddressesList.size(); ++i)
    {
        if(ipAddressesList.at(i) != QHostAddress::LocalHost &&
                ipAddressesList.at(i).toIPv4Address())
        {
            ipAddress = ipAddressesList.at(i).toString();
            break;
        }
    }

    // if we did not find one, use IPv4 localhost
    if(ipAddress.isEmpty())
    {
        ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    }

    qDebug() << tr("The server is running on\n\nIP: %1\nport: %2\n\n")
                .arg(ipAddress)
                .arg(m_tcpServer->serverPort());

    connect(m_tcpServer, &QTcpServer::newConnection,
            this, [=] ()
    {
        emit newSocket(m_tcpServer->nextPendingConnection());
    });
}
