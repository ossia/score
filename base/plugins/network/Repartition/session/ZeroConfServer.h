#pragma once

#include <QtNetwork>
#include <QDebug>
#include <bonjourserviceregister.h>
#include <QtNetwork/QTcpServer>

class ZeroconfServer : public QObject
{
		Q_OBJECT

		quint16 _receiverPort{}; //@todo default port ?
	public:
		void setPort(int receiverPort)
		{
			_receiverPort = receiverPort;
		}

		ZeroconfServer():
			QObject()
		{
			tcpServer = new QTcpServer(this);
			bonjourRegister = new BonjourServiceRegister(this);

			if (!tcpServer->listen(QHostAddress::Any, 42591))
			{
				qDebug() << "Unable to start the server: "
						 << tcpServer->errorString();
				return;
			}

			connect(tcpServer, SIGNAL(newConnection()),
					this,	   SLOT(sendConnectionData()));

			bonjourRegister->registerService(BonjourRecord(tr("Distributed Score on %1").arg(QHostInfo::localHostName()),
														   QLatin1String("_score_net_api._tcp"),
														   QString()),
											 tcpServer->serverPort());

			qDebug() << "ZeroConf server listening on port : " << tcpServer->serverPort();
		}

		virtual ~ZeroconfServer() = default;

	private slots:
		void sendConnectionData()
		{
			QByteArray block;
			QDataStream out(&block, QIODevice::WriteOnly);
			QTcpSocket *clientConnection = tcpServer->nextPendingConnection();

			out.setVersion(QDataStream::Qt_5_2);
			// reserve space to write the size afterwards
			out << (quint16) 0;

			// write the data
			out << clientConnection->localAddress()
				<< (quint16) _receiverPort;

			// write the size at the beginning
			out.device()->seek(0);
			out << (quint16)(block.size() - sizeof(quint16));

			connect(clientConnection, SIGNAL(disconnected()),
					clientConnection, SLOT(deleteLater()));

			clientConnection->write(block);
			clientConnection->disconnectFromHost();
		}

	private:
		QTcpServer *tcpServer;
		BonjourServiceRegister *bonjourRegister;
};
