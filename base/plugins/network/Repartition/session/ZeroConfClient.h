#pragma once

#include <QTcpSocket>
#include <bonjourrecord.h>
#include <bonjourservicebrowser.h>
#include <bonjourserviceresolver.h>
#include <QtNetwork>
#include <QtNetwork/QHostAddress>
#include <QString>
#include <iostream>
#include "ConnectionData.hpp"

class ZeroConfClientThread;
class ZeroConfClient: public QObject
{
		Q_OBJECT
	friend class ZeroConfClientThread;
	public:
		ZeroConfClient():
			QObject()
		{
			// Chargé de recevoir les messages ensuite
			tcpSocket = new QTcpSocket(this);

			connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readConnectionData()));
			connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
					this, SLOT(displayError(QAbstractSocket::SocketError)));

			// Chargé de regarder les sessions ouvertes
			bonjourBrowser = new BonjourServiceBrowser(this);

			connect(bonjourBrowser, SIGNAL(currentBonjourRecordsChanged(const QList<BonjourRecord> &)),
					this, SLOT(updateRecords(const QList<BonjourRecord> &)));

			bonjourBrowser->browseForServiceType(QLatin1String("_score_net_api._tcp"));
		}

		virtual ~ZeroConfClient()
		{
			delete tcpSocket;
			delete bonjourBrowser;
			delete bonjourResolver;
		}

		void connectTo(const BonjourRecord& record)
		{
			blockSize = 0;
			tcpSocket->abort();

			if (!bonjourResolver)
			{
				bonjourResolver = new BonjourServiceResolver(this);
				connect(bonjourResolver, SIGNAL(bonjourRecordResolved(const QHostInfo &, int)),
						this, SLOT(connectToServer(const QHostInfo &, int)));
			}

			bonjourResolver->resolveBonjourRecord(record);
		}

		QList<BonjourRecord> getRecords()
		{
			return _currentRecords;
		}

	signals:
		void setLocalAddress(QHostAddress);
		void connectedTo(QHostAddress, quint16);
		void recordsChanged();

	private slots:
		void updateRecords(const QList<BonjourRecord> &list)
		{
			_currentRecords = list;
			_connectData.clear();
			for(BonjourRecord record : list)
				connectTo(record);
		}

		void readConnectionData()
		{
			QDataStream in(tcpSocket);
			in.setVersion(QDataStream::Qt_5_2);

			if (blockSize == 0)
			{
				if (tcpSocket->bytesAvailable() < (int)sizeof(quint16))
					return;

				in >> blockSize;
			}

			if (tcpSocket->bytesAvailable() < blockSize)
				return;

			quint16 port;
			QHostAddress ip;

			in >> ip >> port;
			_connectData.push_back({ip.toString().toStdString().c_str(), port, "0.0.0.0"}); // TODO recheck this localip
		}

		void displayError(QAbstractSocket::SocketError socketError)
		{
			switch (socketError)
			{
				case QAbstractSocket::RemoteHostClosedError:
					break;
				case QAbstractSocket::HostNotFoundError:
					std::cerr << "Host not found" << std::endl;
					break;
				case QAbstractSocket::ConnectionRefusedError:
					std::cerr << "Connection refused" << std::endl;
					break;
				default:
					qDebug() << tcpSocket->errorString();
			}
		}

		void connectToServer(const QHostInfo &hostInfo, int port)
		{
			const QList<QHostAddress> &addresses = hostInfo.addresses();
			if (!addresses.isEmpty())
			{
				tcpSocket->connectToHost(addresses.first(), port);
			}
		}

	private:
		QTcpSocket *tcpSocket = nullptr;
		quint16 blockSize;
		BonjourServiceBrowser *bonjourBrowser = nullptr;
		BonjourServiceResolver *bonjourResolver = nullptr;

		QList<BonjourRecord> _currentRecords{};

		std::vector<ConnectionData> _connectData;
};
