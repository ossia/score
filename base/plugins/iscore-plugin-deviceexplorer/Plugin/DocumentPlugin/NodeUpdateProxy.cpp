#include "NodeUpdateProxy.hpp"
#include "DeviceDocumentPlugin.hpp"
#include "Plugin/Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/NodePath.hpp>
#include <boost/range/algorithm/find_if.hpp>

NodeUpdateProxy::NodeUpdateProxy(DeviceDocumentPlugin& root):
    m_devModel{root}
{

}

void NodeUpdateProxy::addDevice(const iscore::DeviceSettings& dev)
{
    auto node = new iscore::Node(dev, nullptr);
    m_devModel.createDeviceFromNode(*node);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->addDevice(node);
    }
    else
    {
        m_devModel.rootNode().insertChild(
                    m_devModel.rootNode().childCount(), node);
    }
}

void NodeUpdateProxy::loadDevice(const iscore::Node& node)
{
    m_devModel.createDeviceFromNode(node);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->addDevice(
                    new iscore::Node{node});
    }
    else
    {
        m_devModel.rootNode().insertChild(
                    m_devModel.rootNode().childCount(),
                    new iscore::Node{node});
    }
}

void NodeUpdateProxy::removeDevice(const iscore::DeviceSettings& dev)
{
    m_devModel.list().removeDevice(dev.name);

    if(m_deviceExplorer)
    {
        for(int row = 0; row < m_deviceExplorer->rootNode().childCount(); row++)
        {
            auto index = m_deviceExplorer->index(row, 0, QModelIndex());
            auto node = static_cast<iscore::Node*>(index.internalPointer());

            if(node->isDevice() && node->deviceSettings().name == dev.name)
            {
                m_deviceExplorer->removeRow(row);
                break;
            }
        }
    }
    else
    {
        for(const auto& child : m_devModel.rootNode().children())
        {
            if(child->deviceSettings().name == dev.name)
            {
                m_devModel.rootNode().removeChild(child);
                delete child;
                break;
            }
        }

    }
}

void NodeUpdateProxy::addAddress(
        const NodePath& parentPath,
        const iscore::AddressSettings& settings)
{
    auto parentnode = parentPath.toNode(&m_devModel.rootNode());

    // Add in the device impl
    // Get the device node :
    auto dev_node = m_devModel.rootNode().childAt(parentPath.at(0));
    Q_ASSERT(dev_node->isDevice());

    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                                   settings,
                                   parentnode->address());

    // Add in the device implementation
    m_devModel.list().device(
                dev_node->deviceSettings().name)
            .addAddress(full);

    // Add in the device explorer
    if(m_deviceExplorer)
    {
        m_deviceExplorer->addAddress(
                    parentnode,
                    settings);
    }
    else
    {
        parentnode->insertChild(parentnode->childCount(), new iscore::Node{settings});
    }
}

void NodeUpdateProxy::removeAddress(
        const NodePath& parentPath,
        const iscore::AddressSettings& settings)
{
    iscore::Node* parentnode = parentPath.toNode(&m_devModel.rootNode());

    auto addr = parentnode->address();
    addr.path.append(settings.name);

    // Remove from the device implementation
    auto dev_node = m_devModel.rootNode().childAt(parentPath.at(0));
    m_devModel.list().device(
                dev_node->deviceSettings().name)
            .removeAddress(addr);

    // Remove from the device explorer

    auto it = boost::range::find_if(
                  parentnode->children(),
                  [&] (const iscore::Node* n) { return n->addressSettings().name == settings.name; });
    Q_ASSERT(it != parentnode->children().end());
    auto theNode = *it;
    if(m_deviceExplorer)
    {
        m_deviceExplorer->removeNode(theNode);
    }
    else
    {
        parentnode->removeChild(theNode);
        delete theNode;
    }
}
