#include "NodeUpdateProxy.hpp"
#include "DeviceDocumentPlugin.hpp"
#include "Plugin/Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Address/AddressSettings.hpp>
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

void NodeUpdateProxy::updateDevice(
        const QString &name,
        const iscore::DeviceSettings& dev)
{
    m_devModel.list().device(name).updateSettings(dev);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->updateDevice(name, dev);
    }
    else
    {
        auto it = std::find_if(m_devModel.rootNode().begin(), m_devModel.rootNode().end(),
                               [&] (const iscore::Node& n) { return n.get<iscore::DeviceSettings>().name == name; });

        ISCORE_ASSERT(it != m_devModel.rootNode().end());
        it->set(dev);
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

            if(node
            && node->is<iscore::DeviceSettings>()
            && node->get<iscore::DeviceSettings>().name == dev.name)
            {
                m_deviceExplorer->removeRow(row);
                break;
            }
        }
    }
    else
    {
        auto children_cpy = m_devModel.rootNode().children();
        for(const auto& child : children_cpy)
        {
            if(child->get<iscore::DeviceSettings>().name == dev.name)
            {
                m_devModel.rootNode().removeChild(child);
                delete child;
                break;
            }
        }

    }
}

void NodeUpdateProxy::addAddress(
        const iscore::NodePath& parentPath,
        const iscore::AddressSettings& settings)
{
    auto parentnode = parentPath.toNode(&m_devModel.rootNode());

    // Add in the device impl
    // Get the device node :
    const auto& dev_node = m_devModel.rootNode().childAt(parentPath.at(0));
    ISCORE_ASSERT(dev_node.is<iscore::DeviceSettings>());

    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                                   settings,
                                   iscore::address(*parentnode));

    // Add in the device implementation
    m_devModel
            .list()
            .device(dev_node.get<iscore::DeviceSettings>().name)
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

void NodeUpdateProxy::updateAddress(
        const iscore::NodePath &nodePath,
        const iscore::AddressSettings &settings)
{
    auto node = nodePath.toNode(&m_devModel.rootNode());
    const auto addr = iscore::address(*node);
    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(
                                   settings,
                                   addr);

    // Update in the device implementation
    m_devModel
            .list()
            .device(addr.device)
            .updateAddress(full);

    // Update in the device explorer
    if(m_deviceExplorer)
    {
        m_deviceExplorer->updateAddress(
                    node,
                    settings);
    }
    else
    {
        node->set(settings);
    }
}

void NodeUpdateProxy::removeAddress(
        const iscore::NodePath& parentPath,
        const iscore::AddressSettings& settings)
{
    iscore::Node* parentnode = parentPath.toNode(&m_devModel.rootNode());

    auto addr = iscore::address(*parentnode);
    addr.path.append(settings.name);

    // Remove from the device implementation
    const auto& dev_node = m_devModel.rootNode().childAt(parentPath.at(0));
    m_devModel.list().device(
                dev_node.get<iscore::DeviceSettings>().name)
            .removeAddress(addr);

    // Remove from the device explorer
    auto it = std::find_if(
                  parentnode->begin(), parentnode->end(),
                  [&] (const iscore::Node& n) { return n.get<iscore::AddressSettings>().name == settings.name; });
    ISCORE_ASSERT(it != parentnode->end());

    auto theNode = &*it;
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

void NodeUpdateProxy::updateValue(
        const iscore::Address& addr,
        const iscore::Value& v)
{
    auto n = iscore::try_getNodeFromAddress(m_devModel.rootNode(), addr);
    if(!n->is<iscore::AddressSettings>())
    {
        qDebug() << "Updating invalid node";
        return;
    }
    if(m_deviceExplorer)
    {
        m_deviceExplorer->updateValue(n, v);
    }
    else
    {
        n->get<iscore::AddressSettings>().value = v;
    }
}
