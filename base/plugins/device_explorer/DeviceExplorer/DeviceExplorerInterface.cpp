#include "DeviceExplorerInterface.hpp"

#include "../Plugin/DeviceExplorerPanelFactory.hpp"
#include "../Plugin/Panel/DeviceExplorerModel.hpp"
#include "Node/Node.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>

QString DeviceExplorer::panelName()
{
    return "DeviceExplorerPanelModel";
}

QString DeviceExplorer::explorerName()
{
    return "DeviceExplorerModel";
}


DeviceExplorerModel* DeviceExplorer::getModel(QObject* object)
{
    return getModel(iscore::IDocument::documentFromObject(object));
}


QJsonObject DeviceExplorer::toJson(DeviceExplorerModel* deviceExplorer)
{
    Serializer<JSON> ser;
    if(deviceExplorer->rootNode())
        ser.readFrom(*deviceExplorer->rootNode());
    return ser.m_obj;
}


QByteArray DeviceExplorer::toByteArray(DeviceExplorerModel* deviceExplorer)
{
    QByteArray b;
    Serializer<DataStream> ser(&b);
    if(deviceExplorer->rootNode())
        ser.readFrom(*deviceExplorer->rootNode());
    return b;
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

DeviceExplorerModel *DeviceExplorer::getModel(iscore::Document *doc)
{
    return static_cast<DeviceExplorerPanelModel*>(
                doc->model()
                   ->panel(DeviceExplorer::panelName()))->deviceExplorer();;
}

