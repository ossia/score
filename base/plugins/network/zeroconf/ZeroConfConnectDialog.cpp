#include <QtWidgets/QtWidgets>
#include <QtNetwork>
#include <QtNetwork/QHostAddress>
#include "ZeroConfConnectDialog.hpp"

ZeroconfConnectDialog::ZeroconfConnectDialog(QWidget* parent)
    : QDialog(parent)
{
    BonjourServiceBrowser* bonjourBrowser = new BonjourServiceBrowser(this);

    treeWidget = new QTreeWidget(this);
    treeWidget->setHeaderLabels(QStringList() << tr("Available Servers"));
    connect(bonjourBrowser, SIGNAL(currentBonjourRecordsChanged(const QList<BonjourRecord>&)),
            this, SLOT(updateRecords(const QList<BonjourRecord>&)));

    connectButton = new QPushButton(tr("Connect"));
    connectButton->setDefault(true);
    connectButton->setEnabled(false);

    quitButton = new QPushButton(tr("Back"));

    buttonBox = new QDialogButtonBox;
    buttonBox->addButton(connectButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);

    tcpSocket = new QTcpSocket(this);


    connect(connectButton, SIGNAL(clicked()), this, SLOT(connectTo()));
    connect(quitButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readConnectionData()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError(QAbstractSocket::SocketError)));

    QGridLayout* mainLayout = new QGridLayout;
    mainLayout->addWidget(treeWidget, 0, 0, 2, 2);
    mainLayout->addWidget(buttonBox, 3, 0, 1, 2);
    setLayout(mainLayout);

    setWindowTitle(tr("Connection"));
    treeWidget->setFocus();
    bonjourBrowser->browseForServiceType(QLatin1String("_score_net_api._tcp"));
}

void ZeroconfConnectDialog::connectTo()
{
    blockSize = 0;
    tcpSocket->abort();

    QList<QTreeWidgetItem*> selectedItems = treeWidget->selectedItems();

    if(selectedItems.isEmpty())
    {
        return;
    }

    if(!bonjourResolver)
    {
        bonjourResolver = new BonjourServiceResolver(this);
        connect(bonjourResolver, SIGNAL(bonjourRecordResolved(const QHostInfo&, int)),
                this, SLOT(connectToServer(const QHostInfo&, int)));
    }

    QTreeWidgetItem* item = selectedItems.at(0);
    QVariant variant = item->data(0, Qt::UserRole);
    //qDebug() << "Bonjour record:" << variant.value<BonjourRecord>().serviceName << variant.value<BonjourRecord>().registeredType << variant.value<BonjourRecord>().replyDomain;
    bonjourResolver->resolveBonjourRecord(variant.value<BonjourRecord>());

    close();
}

void ZeroconfConnectDialog::connectToServer(const QHostInfo& hostInfo, int port)
{
    const QList<QHostAddress>& addresses = hostInfo.addresses();

    if(!addresses.isEmpty())
    {
        tcpSocket->connectToHost(addresses.first(), port);
    }
}

void ZeroconfConnectDialog::readConnectionData()
{
    QDataStream in(tcpSocket);
    in.setVersion(QDataStream::Qt_5_2);

    if(blockSize == 0)
    {
        if(tcpSocket->bytesAvailable() < (int) sizeof(quint16))
        {
            return;
        }

        in >> blockSize;
    }

    if(tcpSocket->bytesAvailable() < blockSize)
    {
        return;
    }

    quint16 port;
    QHostAddress ip;
    in >> ip >> port;

    ConnectionData d {ip.toString().toStdString(), port, tcpSocket->localAddress().toString().toStdString() };
    emit connectedTo(d);
}

void ZeroconfConnectDialog::displayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError)
    {
        case QAbstractSocket::RemoteHostClosedError:
            break;

        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, tr("Client"),
                                     tr("The host was not found. Please check the "
                                        "host name and port settings."));
            break;

        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, tr("Client"),
                                     tr("The connection was refused by the peer. "
                                        "Make sure the fortune server is running, "
                                        "and check that the host name and port "
                                        "settings are correct."));
            break;

        default:
            QMessageBox::information(this, tr("Client"),
                                     tr("The following error occurred: %1.")
                                     .arg(tcpSocket->errorString()));
    }
}

void ZeroconfConnectDialog::updateRecords(const QList<BonjourRecord>& list)
{
    treeWidget->clear();
    foreach(BonjourRecord record, list)
    {
        QVariant variant;
        variant.setValue(record);
        QTreeWidgetItem* processItem = new QTreeWidgetItem(treeWidget,
                QStringList() << record.serviceName);
        processItem->setData(0, Qt::UserRole, variant);
    }

    if(treeWidget->invisibleRootItem()->childCount() > 0)
    {
        treeWidget->invisibleRootItem()->child(0)->setSelected(true);
    }

    connectButton->setEnabled(treeWidget->invisibleRootItem()->childCount() != 0);
}
