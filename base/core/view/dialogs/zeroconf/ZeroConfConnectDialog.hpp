/****************************************************************************
**
** Copyright (C) 2004-2007 Trolltech ASA. All rights reserved.
**
** This file is part of the example classes of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef CLIENT_H
#define CLIENT_H

#include <QDialog>
#include <QTcpSocket>
#include <bonjourrecord.h>
#include <bonjourservicebrowser.h>
#include <bonjourserviceresolver.h>

#include <API/Headers/Repartition/session/ConnectionData.hpp>


class QDialogButtonBox;
class QPushButton;
class QTcpSocket;
class QLabel;
class QTreeWidget;
class QHostInfo;
class ZeroconfConnectDialog : public QDialog
{
	Q_OBJECT

public:
	ZeroconfConnectDialog(QWidget *parent = 0);

	signals:
	void setLocalAddress(QHostAddress);
	void connectedTo(ConnectionData);

private slots:
	void updateRecords(const QList<BonjourRecord> &list);
	void connectTo();
	void readConnectionData();
	void displayError(QAbstractSocket::SocketError socketError);
	void connectToServer(const QHostInfo &hostInfo, int);

private:
	QLabel *statusLabel = nullptr;
	QPushButton *connectButton = nullptr;
	QPushButton *quitButton = nullptr;
	QDialogButtonBox *buttonBox = nullptr;

	QTcpSocket *tcpSocket = nullptr;
	quint16 blockSize;
	BonjourServiceBrowser *bonjourBrowser = nullptr;
	BonjourServiceResolver *bonjourResolver = nullptr;
	QTreeWidget *treeWidget = nullptr;
};

#endif
