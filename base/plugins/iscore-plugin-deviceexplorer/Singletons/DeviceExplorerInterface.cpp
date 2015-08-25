#include "DeviceExplorerInterface.hpp"

#include "../Plugin/DeviceExplorerPanelFactory.hpp"
#include "../Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>
#include <core/document/DocumentModel.hpp>
#include <boost/range/algorithm.hpp>

using namespace iscore;
QString DeviceExplorer::panelName()
{
    return "DeviceExplorerPanelModel";
}

QString DeviceExplorer::explorerName()
{
    return "DeviceExplorerModel";
}

Address DeviceExplorer::addressFromModelIndex(const QModelIndex& m)
{
    if(!m.isValid())
        return Address();

    QModelIndex index = m;
    Address a;

    auto node = static_cast<Node*>(index.internalPointer());
    while(node && !node->is<DeviceSettings>())
    {
        a.path.append(node->get<iscore::AddressSettings>().name);
        node = node->parent();
    }

    boost::range::reverse(a.path);
    ISCORE_ASSERT(node);
    ISCORE_ASSERT(node->is<DeviceSettings>());
    a.device = node->get<DeviceSettings>().name;

    return a;
}


Message DeviceExplorer::messageFromModelIndex(const QModelIndex& m)
{
    Message mess;
    mess.address = addressFromModelIndex(m);

    auto node = static_cast<Node*>(m.internalPointer());
    ISCORE_ASSERT(!node->is<DeviceSettings>());

    mess.value = node->get<iscore::AddressSettings>().value;

    return mess;
}
