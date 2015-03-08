#include "DeviceExplorerInterface.hpp"

#include "../DeviceExplorerPanelFactory.hpp"
#include "../Panel/DeviceExplorerModel.hpp"
#include "iscore/document/DocumentInterface.hpp"
#include "core/document/Document.hpp"
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include "../Panel/Node.hpp"

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
    return nodeToJson(deviceExplorer->rootNode());
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


DeviceExplorerModel *DeviceExplorer::getModel(iscore::Document *doc)
{
    return static_cast<DeviceExplorerPanelModel*>(
                doc->model()
                   ->panel(DeviceExplorer::panelName()))->deviceExplorer();;
}
