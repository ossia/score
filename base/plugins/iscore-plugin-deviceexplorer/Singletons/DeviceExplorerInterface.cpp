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

iscore::Address DeviceExplorer::addressFromNode(const iscore::Node& m)
{
    iscore::Address a;
    auto node = &m;

    while(node && !node->is<iscore::DeviceSettings>())
    {
        a.path.append(node->get<iscore::AddressSettings>().name);
        node = node->parent();
    }

    boost::range::reverse(a.path);
    ISCORE_ASSERT(node);
    ISCORE_ASSERT(node->is<iscore::DeviceSettings>());
    a.device = node->get<iscore::DeviceSettings>().name;

    return a;
}


iscore::Message DeviceExplorer::messageFromNode(const iscore::Node& node)
{
    if(!node.is<iscore::AddressSettings>())
        return {};

    iscore::Message mess;
    mess.address = addressFromNode(node);
    mess.value = node.get<iscore::AddressSettings>().value;

    return mess;
}



iscore::Address DeviceExplorer::addressFromModelIndex(const QModelIndex& index)
{
    if(!index.isValid())
        return {};

    auto node = static_cast<iscore::Node*>(index.internalPointer());
    ISCORE_ASSERT(node);
    return addressFromNode(*node);
}


iscore::Message DeviceExplorer::messageFromModelIndex(const QModelIndex& index)
{
    if(!index.isValid())
        return {};

    auto node = static_cast<iscore::Node*>(index.internalPointer());
    ISCORE_ASSERT(node);
    return messageFromNode(*node);
}
