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
        return {};

    QModelIndex index = m;
    Address a;

    auto node = static_cast<Node*>(index.internalPointer());
    while(node && !node->isDevice())
    {
        a.path.append(node->addressSettings().name);
        node = node->parent();
    }

    boost::range::reverse(a.path);
    Q_ASSERT(node);
    Q_ASSERT(node->isDevice());
    a.device = node->deviceSettings().name;

    return a;
}


Message DeviceExplorer::messageFromModelIndex(const QModelIndex& m)
{
    Message mess;
    mess.address = addressFromModelIndex(m);
    mess.value = m.sibling(m.row(), 1).data();

    return mess;
}
