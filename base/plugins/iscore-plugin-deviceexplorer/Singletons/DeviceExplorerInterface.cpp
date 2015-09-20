#include "DeviceExplorerInterface.hpp"

#include "../Plugin/DeviceExplorerPanelFactory.hpp"
#include "../Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <core/document/DocumentModel.hpp>
#include <boost/range/algorithm.hpp>

QString DeviceExplorer::panelName()
{
    return "DeviceExplorerPanelModel";
}

QString DeviceExplorer::explorerName()
{
    return "DeviceExplorerModel";
}


iscore::Address DeviceExplorer::addressFromModelIndex(const QModelIndex& index)
{
    if(!index.isValid())
        return {"", {""}};

    auto node = static_cast<iscore::Node*>(index.internalPointer());
    ISCORE_ASSERT(node);
    return iscore::address(*node);
}


iscore::Message DeviceExplorer::messageFromModelIndex(const QModelIndex& index)
{
    if(!index.isValid())
        return {};

    auto node = static_cast<iscore::Node*>(index.internalPointer());
    ISCORE_ASSERT(node);
    return iscore::message(*node);
}
