#pragma once
#include <QtCore/QSocketNotifier>
#include <dns_sd.h>
#include <QDebug>
#include "bonjourrecord.h"

class BonjourServiceBrowser : public QObject
{
		Q_OBJECT
	public:
		BonjourServiceBrowser(QObject *parent = 0):
			QObject(parent)
		{
		}

		~BonjourServiceBrowser()
		{
			if (dnssref)
			{
				DNSServiceRefDeallocate(dnssref);
				delete bonjourSocket;
			}
		}

		void browseForServiceType(const QString &serviceType)
		{
			DNSServiceErrorType err = DNSServiceBrowse(&dnssref, 0, 0, serviceType.toUtf8().constData(), 0,
													   bonjourBrowseReply, this);
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

		QList<BonjourRecord> currentRecords() const
		{
			return bonjourRecords;
		}

		QString serviceType() const
		{
			return browsingType;
		}

	signals:
		void currentBonjourRecordsChanged(const QList<BonjourRecord> &list);
		void error(DNSServiceErrorType err);

	private slots:
		void bonjourSocketReadyRead()
		{
			DNSServiceErrorType err = DNSServiceProcessResult(dnssref);
			if (err != kDNSServiceErr_NoError)
				emit error(err);
		}

	private:
		static void DNSSD_API bonjourBrowseReply(DNSServiceRef , DNSServiceFlags flags, quint32,
												 DNSServiceErrorType errorCode, const char *serviceName,
												 const char *regType, const char *replyDomain, void *context)
		{
			BonjourServiceBrowser *serviceBrowser = static_cast<BonjourServiceBrowser *>(context);
			if (errorCode != kDNSServiceErr_NoError)
			{
				emit serviceBrowser->error(errorCode);
			}
			else
			{
				BonjourRecord bonjourRecord(serviceName, regType, replyDomain);
				if (flags & kDNSServiceFlagsAdd) {
					if (!serviceBrowser->bonjourRecords.contains(bonjourRecord))
						serviceBrowser->bonjourRecords.append(bonjourRecord);
				}
				else
				{
					serviceBrowser->bonjourRecords.removeAll(bonjourRecord);
				}
				if (!(flags & kDNSServiceFlagsMoreComing))
				{
					emit serviceBrowser->currentBonjourRecordsChanged(serviceBrowser->bonjourRecords);
				}
			}
		}

		DNSServiceRef dnssref = 0;
		QSocketNotifier *bonjourSocket = nullptr;
		QList<BonjourRecord> bonjourRecords;
		QString browsingType;
};
