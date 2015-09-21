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
    iscore::Node node(dev, nullptr);
    auto newNode = m_devModel.createDeviceFromNode(node);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->addDevice(std::move(newNode));
    }
    else
    {
        m_devModel.rootNode().push_back(std::move(newNode));
    }
}

void NodeUpdateProxy::loadDevice(const iscore::Node& node)
{
    auto n = m_devModel.createDeviceFromNode(node);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->addDevice(std::move(node));
    }
    else
    {
        m_devModel.rootNode().push_back(std::move(node));
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

    for(auto it = m_devModel.rootNode().begin(); it < m_devModel.rootNode().end(); ++it)
    {
        if(it->is<iscore::DeviceSettings>() && it->get<iscore::DeviceSettings>().name == dev.name)
        {
            if(m_deviceExplorer)
            {
                m_deviceExplorer->removeNode(it);
            }
            else
            {
                m_devModel.rootNode().removeChild(it);
            }
        }
    }
}

void NodeUpdateProxy::addAddress(
        const iscore::NodePath& parentPath,
        const iscore::AddressSettings& settings,
        int row)
{
    auto parentnode = parentPath.toNode(&m_devModel.rootNode());

    // Add in the device impl
    // Get the device node :
    // TODO row isn't managed here.
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
                    settings,
                    row);
    }
    else
    {
        parentnode->emplace(parentnode->begin() + row, settings, parentnode);
    }
}

void NodeUpdateProxy::rec_addNode(
        iscore::NodePath parentPath,
        const iscore::Node& n,
        int row)
{
    addAddress(parentPath, n.get<iscore::AddressSettings>(), row);

    parentPath.append(row);

    int r = 0;
    for(const auto& child : n.children())
    {
        rec_addNode(parentPath, child, r++);
    }
}

void NodeUpdateProxy::addNode(
        const iscore::NodePath& parentPath,
        const iscore::Node& node,
        int row)
{
    ISCORE_ASSERT(node.is<iscore::AddressSettings>());

    rec_addNode(parentPath, node, row);
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

void NodeUpdateProxy::removeNode(
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
            .removeNode(addr);

    // Remove from the device explorer
    auto it = std::find_if(
                  parentnode->begin(), parentnode->end(),
                  [&] (const iscore::Node& n) { return n.get<iscore::AddressSettings>().name == settings.name; });
    ISCORE_ASSERT(it != parentnode->end());

    if(m_deviceExplorer)
    {
        m_deviceExplorer->removeNode(it);
    }
    else
    {
        parentnode->removeChild(it);
    }
}

void NodeUpdateProxy::updateLocalValue(
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

void NodeUpdateProxy::updateRemoteValue(
        const iscore::Address& addr,
        const iscore::Value& val)
{
    // TODO add these checks everywhere.
    if(m_devModel.list().hasDevice(addr.device))
    {
        // Update in the device implementation
        m_devModel
                .list()
                .device(addr.device)
                .sendMessage({addr, val});
    }
}

void rec_updateRemoteValues(iscore::Node& n, DeviceInterface& dev)
{
    // OPTIMIZEME
    auto val = dev.refresh(iscore::address(n));
    if(val)
        n.get<iscore::AddressSettings>().value = *val;

    for(auto& child : n)
    {
        rec_updateRemoteValues(child, dev);
    }
}

void NodeUpdateProxy::updateRemoteValues(const iscore::NodeList& nodes)
{
    // For each node, get its device.
    for(auto n : nodes)
    {
        if(n->is<iscore::DeviceSettings>())
        {
            auto dev_name = n->get<iscore::DeviceSettings>().name;
            auto& dev = m_devModel.list().device(dev_name);
            if(!dev.canRefresh())
                continue;

            for(auto& child : *n)
            {
                rec_updateRemoteValues(child, dev);
            }
        }
        else
        {
            auto addr = iscore::address(*n);
            auto& dev = m_devModel.list().device(addr.device);
            if(!dev.canRefresh())
                continue;

            rec_updateRemoteValues(*n, dev);
        }
    }
}
