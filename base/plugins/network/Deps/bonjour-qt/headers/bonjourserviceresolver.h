#pragma once
#include <QDebug>

#include <QtCore/QSocketNotifier>
#include <QtNetwork/QHostInfo>

#include <dns_sd.h>
#include <QDebug>

#include "bonjourrecord.h"

class BonjourServiceResolver : public QObject
{
		Q_OBJECT
	public:
		BonjourServiceResolver(QObject *parent):
			QObject(parent)
		{
		}

		~BonjourServiceResolver()
		{
			if (dnssref)
			{
				DNSServiceRefDeallocate(dnssref);
				delete bonjourSocket;
			}
		}

		void resolveBonjourRecord(const BonjourRecord &record)
		{
			if (dnssref)
			{
				qWarning("resolve in process, aborting");
				return;
			}

			DNSServiceErrorType err = DNSServiceResolve(&dnssref, 0, 0,
														record.serviceName.toUtf8().constData(),
														record.registeredType.toUtf8().constData(),
														record.replyDomain.toUtf8().constData(),
														(DNSServiceResolveReply)bonjourResolveReply, this);
			if (err != kDNSServiceErr_NoError)
			{
				qDebug() << err;
				emit error(err);
			}
			else
			{
				int sockfd = DNSServiceRefSockFD(dnssref);
				if (sockfd == -1)
				{
					qDebug() << kDNSServiceErr_Invalid;
					emit error(kDNSServiceErr_Invalid);
				}
				else
				{
					qDebug("ok");
					bonjourSocket = new QSocketNotifier(sockfd, QSocketNotifier::Read, this);
					connect(bonjourSocket, SIGNAL(activated(int)), this, SLOT(bonjourSocketReadyRead()));
				}
			}
		}

	signals:
		void bonjourRecordResolved(const QHostInfo &hostInfo, int port);
		void error(DNSServiceErrorType error);

	private slots:

		void cleanupResolve()
		{
			if (dnssref)
			{
				DNSServiceRefDeallocate(dnssref);
				dnssref = nullptr;
				delete bonjourSocket;
				bonjourSocket = nullptr;
			}
		}

		void bonjourSocketReadyRead()
		{
			DNSServiceErrorType err = DNSServiceProcessResult(dnssref);
			if (err != kDNSServiceErr_NoError)
			{
				qDebug() << err;
				emit error(err);
			}
		}

		void finishConnect(const QHostInfo &hostInfo)
		{
			emit bonjourRecordResolved(hostInfo, bonjourPort);
			QMetaObject::invokeMethod(this, "cleanupResolve", Qt::QueuedConnection);
		}

	private:
		static void DNSSD_API bonjourResolveReply(DNSServiceRef, DNSServiceFlags,
												  quint32, DNSServiceErrorType errorCode,
												  const char *, const char *hosttarget, quint16 port,
												  quint16 , const char *, void *context)
		{
			qDebug(Q_FUNC_INFO);
			BonjourServiceResolver *serviceResolver = static_cast<BonjourServiceResolver *>(context);
			if (errorCode != kDNSServiceErr_NoError)
			{
				emit serviceResolver->error(errorCode);
				return;
			}

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
			{
				port =  0 | ((port & 0x00ff) << 8) | ((port & 0xff00) >> 8);
			}
#endif
			serviceResolver->bonjourPort = port;
			QHostInfo::lookupHost(QString::fromUtf8(hosttarget),
								  serviceResolver, SLOT(finishConnect(const QHostInfo &)));
		}

		DNSServiceRef dnssref = 0;
		QSocketNotifier *bonjourSocket = nullptr;
		int bonjourPort = -1;
};

