#pragma once
#include <QtCore/QSocketNotifier>

#include <dns_sd.h>

#include "bonjourrecord.h"

class BonjourServiceRegister : public QObject
{
    Q_OBJECT
public:
		BonjourServiceRegister(QObject *parent = 0):
			QObject(parent)
		{
		}

		~BonjourServiceRegister()
		{
			if (dnssref)
			{
				DNSServiceRefDeallocate(dnssref);
				delete bonjourSocket;
			}
		}

		void registerService(const BonjourRecord &record, quint16 servicePort)
		{
			if (dnssref)
			{
				qWarning("Warning: Already registered a service for this object, aborting new register");
				return;
			}

			quint16 bigEndianPort = servicePort;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
			{
				bigEndianPort =  0 | ((servicePort & 0x00ff) << 8) | ((servicePort & 0xff00) >> 8);
			}
#endif

			DNSServiceErrorType err = DNSServiceRegister(&dnssref, 0, 0, record.serviceName.toUtf8().constData(),
														 record.registeredType.toUtf8().constData(),
														 record.replyDomain.isEmpty() ? 0
																					  : record.replyDomain.toUtf8().constData(),
														 0, bigEndianPort, 0, 0, bonjourRegisterService, this);
			if (err != kDNSServiceErr_NoError)
			{
				emit error(err);
			}
			else
			{
				int sockfd = DNSServiceRefSockFD(dnssref);
				if (sockfd == -1)
				{
					emit error(kDNSServiceErr_Invalid);
				}
				else
				{
					bonjourSocket = new QSocketNotifier(sockfd, QSocketNotifier::Read, this);
					connect(bonjourSocket, SIGNAL(activated(int)), this, SLOT(bonjourSocketReadyRead()));
				}
			}
		}
	BonjourRecord registeredRecord() const
	{
		return finalRecord;
	}

signals:
    void error(DNSServiceErrorType error);
    void serviceRegistered(const BonjourRecord &record);

private slots:
	void bonjourSocketReadyRead()
	{
		DNSServiceErrorType err = DNSServiceProcessResult(dnssref);
		if (err != kDNSServiceErr_NoError)
			emit error(err);
	}

private:
	static void DNSSD_API bonjourRegisterService(DNSServiceRef /*sdRef*/, DNSServiceFlags,
												 DNSServiceErrorType errorCode, const char *name,
												 const char *regtype, const char *domain,
												 void *data)
	{
		BonjourServiceRegister *serviceRegister = static_cast<BonjourServiceRegister *>(data);
		if (errorCode != kDNSServiceErr_NoError)
		{
			emit serviceRegister->error(errorCode);
		}
		else
		{
			serviceRegister->finalRecord = BonjourRecord(QString::fromUtf8(name),
														 QString::fromUtf8(regtype),
														 QString::fromUtf8(domain));
			emit serviceRegister->serviceRegistered(serviceRegister->finalRecord);
		}
	}

	DNSServiceRef dnssref = 0;
	QSocketNotifier *bonjourSocket = nullptr;
    BonjourRecord finalRecord;
};

