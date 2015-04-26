#include "DeviceExplorerInterface.hpp"

#include "../Plugin/DeviceExplorerPanelFactory.hpp"
#include "../Plugin/Panel/DeviceExplorerModel.hpp"

#include <core/document/DocumentModel.hpp>

QString DeviceExplorer::panelName()
{
    return "DeviceExplorerPanelModel";
}

QString DeviceExplorer::explorerName()
{
    return "DeviceExplorerModel";
}

QString DeviceExplorer::addressFromModelIndex(const QModelIndex& m)
{
    QModelIndex index = m;
    QString txt;

    while(index.isValid())
    {
        txt.prepend(QString("/%1")
                    .arg(index.data(0).toString()));
        index = index.parent();
    }

    return txt;
}


Message DeviceExplorer::messageFromModelIndex(const QModelIndex& m)
{
    Message mess;
    mess.address = addressFromModelIndex(m);
    mess.value = m.sibling(m.row(), 1).data();

    return mess;
}
