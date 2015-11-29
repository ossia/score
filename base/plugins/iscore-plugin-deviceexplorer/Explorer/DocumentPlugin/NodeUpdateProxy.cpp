#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <boost/optional/optional.hpp>
#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <vector>

#include "Device/Node/DeviceNode.hpp"
#include "Device/Protocol/DeviceInterface.hpp"
#include "Device/Protocol/DeviceList.hpp"
#include "Device/Protocol/DeviceSettings.hpp"
#include "DeviceDocumentPlugin.hpp"
#include "NodeUpdateProxy.hpp"
#include <State/Address.hpp>
#include <iscore/tools/TreeNode.hpp>

NodeUpdateProxy::NodeUpdateProxy(DeviceDocumentPlugin& root):
    devModel{root}
{

}

void NodeUpdateProxy::addDevice(const iscore::DeviceSettings& dev)
{
    iscore::Node node(dev, nullptr);
    auto newNode = devModel.createDeviceFromNode(node);

    if(deviceExplorer)
    {
        deviceExplorer->addDevice(std::move(newNode));
    }
    else
    {
        devModel.rootNode().push_back(std::move(newNode));
    }
}

void NodeUpdateProxy::loadDevice(const iscore::Node& node)
{
    auto n = devModel.loadDeviceFromNode(node);

    if(deviceExplorer)
    {
        deviceExplorer->addDevice(std::move(node));
    }
    else
    {
        devModel.rootNode().push_back(std::move(node));
    }
}

void NodeUpdateProxy::updateDevice(
        const QString &name,
        const iscore::DeviceSettings& dev)
{
    devModel.list().device(name).updateSettings(dev);

    if(deviceExplorer)
    {
        deviceExplorer->updateDevice(name, dev);
    }
    else
    {
        auto it = std::find_if(devModel.rootNode().begin(), devModel.rootNode().end(),
                               [&] (const iscore::Node& n) { return n.get<iscore::DeviceSettings>().name == name; });

        ISCORE_ASSERT(it != devModel.rootNode().end());
        it->set(dev);
    }
}

void NodeUpdateProxy::removeDevice(const iscore::DeviceSettings& dev)
{
    devModel.list().removeDevice(dev.name);

    for(auto it = devModel.rootNode().begin(); it < devModel.rootNode().end(); ++it)
    {
        if(it->is<iscore::DeviceSettings>() && it->get<iscore::DeviceSettings>().name == dev.name)
        {
            if(deviceExplorer)
            {
                deviceExplorer->removeNode(it);
            }
            else
            {
                devModel.rootNode().removeChild(it);
            }
        }
    }
}

void NodeUpdateProxy::addAddress(
        const iscore::NodePath& parentPath,
        const iscore::AddressSettings& settings,
        int row)
{
    auto parentnode = parentPath.toNode(&devModel.rootNode());

    // Add in the device impl
    // Get the device node :
    // TODO row isn't managed here.
    const auto& dev_node = devModel.rootNode().childAt(parentPath.at(0));
    ISCORE_ASSERT(dev_node.is<iscore::DeviceSettings>());

    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                                   settings,
                                   iscore::address(*parentnode));

    // Add in the device implementation
    devModel
            .list()
            .device(dev_node.get<iscore::DeviceSettings>().name)
            .addAddress(full);

    // Add in the device explorer
    if(deviceExplorer)
    {
        deviceExplorer->addAddress(
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
    auto node = nodePath.toNode(&devModel.rootNode());
    const auto addr = iscore::address(*node);
    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(
                                   settings,
                                   addr);

    // Update in the device implementation
    devModel
            .list()
            .device(addr.device)
            .updateAddress(full);

    // Update in the device explorer
    if(deviceExplorer)
    {
        deviceExplorer->updateAddress(
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
    iscore::Node* parentnode = parentPath.toNode(&devModel.rootNode());

    auto addr = iscore::address(*parentnode);
    addr.path.append(settings.name);

    // Remove from the device implementation
    const auto& dev_node = devModel.rootNode().childAt(parentPath.at(0));
    devModel.list().device(
                dev_node.get<iscore::DeviceSettings>().name)
            .removeNode(addr);

    // Remove from the device explorer
    auto it = std::find_if(
                  parentnode->begin(), parentnode->end(),
                  [&] (const iscore::Node& n) { return n.get<iscore::AddressSettings>().name == settings.name; });
    ISCORE_ASSERT(it != parentnode->end());

    if(deviceExplorer)
    {
        deviceExplorer->removeNode(it);
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
    auto n = iscore::try_getNodeFromAddress(devModel.rootNode(), addr);
    if(!n->is<iscore::AddressSettings>())
    {
        qDebug() << "Updating invalid node";
        return;
    }
    if(deviceExplorer)
    {
        deviceExplorer->updateValue(n, v);
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
    if(devModel.list().hasDevice(addr.device))
    {
        // Update in the device implementation
        devModel
                .list()
                .device(addr.device)
                .sendMessage({addr, val});
    }
}

iscore::Value NodeUpdateProxy::refreshRemoteValue(const iscore::Address& addr)
{
    // TODO here and in the following function, we should still update
    // the device explorer.
    auto dev_it = devModel.list().find(addr.device);
    if(dev_it == devModel.list().devices().end())
        return {};

    auto& dev = **dev_it;

    auto& n = iscore::getNodeFromAddress(devModel.rootNode(), addr).get<iscore::AddressSettings>();
    if(dev.canRefresh())
    {
        if(auto val = dev.refresh(addr))
        {
            n.value = *val;
        }
    }

    return n.value;
}

void rec_refreshRemoteValues(iscore::Node& n, DeviceInterface& dev)
{
    // OPTIMIZEME
    auto val = dev.refresh(iscore::address(n));
    if(val)
        n.get<iscore::AddressSettings>().value = *val;

    for(auto& child : n)
    {
        rec_refreshRemoteValues(child, dev);
    }
}

void NodeUpdateProxy::refreshRemoteValues(const iscore::NodeList& nodes)
{
    // For each node, get its device.
    for(auto n : nodes)
    {
        if(n->is<iscore::DeviceSettings>())
        {
            auto dev_name = n->get<iscore::DeviceSettings>().name;
            auto& dev = devModel.list().device(dev_name);
            if(!dev.canRefresh())
                continue;

            for(auto& child : *n)
            {
                rec_refreshRemoteValues(child, dev);
            }
        }
        else
        {
            auto addr = iscore::address(*n);
            auto& dev = devModel.list().device(addr.device);
            if(!dev.canRefresh())
                continue;

            rec_refreshRemoteValues(*n, dev);
        }
    }
}
