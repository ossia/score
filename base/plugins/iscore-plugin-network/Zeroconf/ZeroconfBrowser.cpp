#include <dnssd/remoteservice.h>
#include <dnssd/servicebrowser.h>
#include <dnssd/servicemodel.h>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFlags>
#include <QGridLayout>
#include <QHostAddress>
#include <QHostInfo>
#include <QList>
#include <QListView>

#include <QVariant>

#include "ZeroconfBrowser.hpp"

class QWidget;

using namespace KDNSSD;
ZeroconfBrowser::ZeroconfBrowser(
        const QString& service,
        QWidget* parent):
    m_dialog{new QDialog{parent}}
{
    QGridLayout* lay = new QGridLayout;
    auto buttonBox = new QDialogButtonBox(
                         QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    lay->addWidget(buttonBox);
    m_dialog->setLayout(lay);


    auto serviceModel = new ServiceModel(
                         new ServiceBrowser(service));
    m_list = new QListView;
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setModel(serviceModel);
    lay->addWidget(m_list);
}

QAction* ZeroconfBrowser::makeAction()
{
    QAction* act = new QAction(tr("Browse for server"), this);
    connect(act, &QAction::triggered,
            m_dialog, [=] { m_dialog->exec(); });

    return act;
}

void ZeroconfBrowser::accept()
{
    auto selection = m_list->currentIndex();
    RemoteService::Ptr service =
            selection
            .data(ServiceModel::ServicePtrRole)
            .value<RemoteService::Ptr>();

    if(service)
    {
        RemoteService* data = service.data();
        data->resolve();
        auto ipAddressesList = QHostInfo::fromName(data->hostName()).addresses();
        QString ipAddress;

        for(int i = 0; i < ipAddressesList.size(); ++i)
        {
            if(ipAddressesList.at(i).toIPv4Address())
            {
                ipAddress = ipAddressesList.at(i).toString();
                break;
            }
        }

        qDebug() << data->isResolved()<< ipAddress << data->port();
        emit sessionSelected(ipAddress, data->port());
        m_dialog->close();
    }
}

void ZeroconfBrowser::reject()
{
    m_dialog->close();
}
