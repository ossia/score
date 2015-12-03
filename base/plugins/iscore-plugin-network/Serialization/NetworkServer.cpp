#include <QDebug>
#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QString>
#include <QTcpServer>

#include "NetworkServer.hpp"


NetworkServer::NetworkServer(int port, QObject* parent):
    QObject{parent}
{
    m_tcpServer = new QTcpServer(this);


    while(!m_tcpServer->listen(QHostAddress::Any, port))
    {
        port++;
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

    qDebug() << "Server: " << ipAddress << ":" << m_tcpServer->serverPort();

    connect(m_tcpServer, &QTcpServer::newConnection,
            this, [=] ()
    {
        emit newSocket(m_tcpServer->nextPendingConnection());
    });
}

int NetworkServer::port() const
{
    return m_tcpServer->serverPort();
}
