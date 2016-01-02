#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <boost/optional/optional.hpp>
#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <vector>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "DeviceDocumentPlugin.hpp"
#include "NodeUpdateProxy.hpp"
#include <State/Address.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/std/Algorithms.hpp>

NodeUpdateProxy::NodeUpdateProxy(DeviceDocumentPlugin& root):
    devModel{root}
{

}

void NodeUpdateProxy::addDevice(const Device::DeviceSettings& dev)
{
    Device::Node node(dev, nullptr);
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

void NodeUpdateProxy::loadDevice(const Device::Node& node)
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
        const Device::DeviceSettings& dev)
{
    devModel.list().device(name).updateSettings(dev);

    if(deviceExplorer)
    {
        deviceExplorer->updateDevice(name, dev);
    }
    else
    {
        auto it = std::find_if(devModel.rootNode().begin(), devModel.rootNode().end(),
                               [&] (const Device::Node& n) { return n.get<Device::DeviceSettings>().name == name; });

        ISCORE_ASSERT(it != devModel.rootNode().end());
        it->set(dev);
    }
}

void NodeUpdateProxy::removeDevice(const Device::DeviceSettings& dev)
{
    devModel.list().removeDevice(dev.name);

    const auto& rootNode = devModel.rootNode();
    auto it = find_if(rootNode, [&] (const auto& val) {
        return val.template is<Device::DeviceSettings>() && val.template get<Device::DeviceSettings>().name == dev.name;
    });
    ISCORE_ASSERT(it != rootNode.end());

    if(deviceExplorer)
    {
        deviceExplorer->removeNode(it);
    }
    else
    {
        devModel.rootNode().erase(it);
    }
}

void NodeUpdateProxy::addAddress(
        const Device::NodePath& parentPath,
        const Device::AddressSettings& settings,
        int row)
{
    auto parentnode = parentPath.toNode(&devModel.rootNode());
    if(!parentnode)
        return;

    // Add in the device impl
    // Get the device node :
    // TODO row isn't managed here.
    const auto& dev_node = devModel.rootNode().childAt(parentPath.at(0));
    ISCORE_ASSERT(dev_node.is<Device::DeviceSettings>());

    // Make a full path
    Device::FullAddressSettings full = Device::FullAddressSettings::make<Device::FullAddressSettings::as_parent>(
                                   settings,
                                   Device::address(*parentnode));

    // Add in the device implementation
    devModel
            .list()
            .device(dev_node.get<Device::DeviceSettings>().name)
            .addAddress(full);

    // Add in the device explorer
    addLocalAddress(*parentnode, settings, row);
}

void NodeUpdateProxy::rec_addNode(
        Device::NodePath parentPath,
        const Device::Node& n,
        int row)
{
    addAddress(parentPath, n.get<Device::AddressSettings>(), row);

    parentPath.append(row);

    int r = 0;
    for(const auto& child : n.children())
    {
        rec_addNode(parentPath, child, r++);
    }
}

void NodeUpdateProxy::addNode(
        const Device::NodePath& parentPath,
        const Device::Node& node,
        int row)
{
    ISCORE_ASSERT(node.is<Device::AddressSettings>());

    rec_addNode(parentPath, node, row);
}

void NodeUpdateProxy::updateAddress(
        const Device::NodePath &nodePath,
        const Device::AddressSettings &settings)
{
    auto node = nodePath.toNode(&devModel.rootNode());
    if(!node)
        return;

    const auto addr = Device::address(*node);
    // Make a full path
    Device::FullAddressSettings full = Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(
                                   settings,
                                   addr);
    full.address.path.last() = settings.name;

    // Update in the device implementation
    devModel
            .list()
            .device(addr.device)
            .updateAddress(addr, full);

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
        const Device::NodePath& parentPath,
        const Device::AddressSettings& settings)
{
    Device::Node* parentnode = parentPath.toNode(&devModel.rootNode());
    if(!parentnode)
        return;

    auto addr = Device::address(*parentnode);
    addr.path.append(settings.name);

    // Remove from the device implementation
    const auto& dev_node = devModel.rootNode().childAt(parentPath.at(0));
    devModel.list().device(
                dev_node.get<Device::DeviceSettings>().name)
            .removeNode(addr);

    // Remove from the device explorer
    auto it = std::find_if(
                  parentnode->begin(), parentnode->end(),
                  [&] (const Device::Node& n) { return n.get<Device::AddressSettings>().name == settings.name; });
    ISCORE_ASSERT(it != parentnode->end());

    if(deviceExplorer)
    {
        deviceExplorer->removeNode(it);
    }
    else
    {
        parentnode->erase(it);
    }
}


void NodeUpdateProxy::addLocalAddress(
        Device::Node& parentnode,
        const Device::AddressSettings& settings,
        int row)
{
    if(deviceExplorer)
    {
        deviceExplorer->addAddress(
                    &parentnode,
                    settings,
                    row);
    }
    else
    {
        parentnode.emplace(parentnode.begin() + row, settings, &parentnode);
    }
}

void NodeUpdateProxy::updateLocalValue(
        const State::Address& addr,
        const State::Value& v)
{
    auto n = Device::try_getNodeFromAddress(devModel.rootNode(), addr);
    if(!n)
        return;

    if(!n->is<Device::AddressSettings>())
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
        n->get<Device::AddressSettings>().value = v;
    }
}

void NodeUpdateProxy::updateLocalSettings(
        const State::Address& addr,
        const Device::AddressSettings& set)
{
    auto n = Device::try_getNodeFromAddress(devModel.rootNode(), addr);
    if(!n)
        return;

    if(!n->is<Device::AddressSettings>())
    {
        qDebug() << "Updating invalid node";
        return;
    }

    if(deviceExplorer)
    {
        deviceExplorer->updateAddress(n, set);
    }
    else
    {
        n->set(set);
    }
}

void NodeUpdateProxy::updateRemoteValue(
        const State::Address& addr,
        const State::Value& val)
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

State::Value NodeUpdateProxy::refreshRemoteValue(const State::Address& addr)
{
    // TODO here and in the following function, we should still update
    // the device explorer.
    auto dev_it = devModel.list().find(addr.device);
    if(dev_it == devModel.list().devices().end())
        return {};

    auto& dev = **dev_it;

    auto& n = Device::getNodeFromAddress(devModel.rootNode(), addr).get<Device::AddressSettings>();
    if(dev.capabilities().canRefresh)
    {
        if(auto val = dev.refresh(addr))
        {
            n.value = *val;
        }
    }

    return n.value;
}

void rec_refreshRemoteValues(Device::Node& n, DeviceInterface& dev)
{
    // OPTIMIZEME
    auto val = dev.refresh(Device::address(n));
    if(val)
        n.get<Device::AddressSettings>().value = *val;

    for(auto& child : n)
    {
        rec_refreshRemoteValues(child, dev);
    }
}

void NodeUpdateProxy::refreshRemoteValues(const Device::NodeList& nodes)
{
    // For each node, get its device.
    for(auto n : nodes)
    {
        if(n->is<Device::DeviceSettings>())
        {
            auto dev_name = n->get<Device::DeviceSettings>().name;
            auto& dev = devModel.list().device(dev_name);
            if(!dev.capabilities().canRefresh)
                continue;

            for(auto& child : *n)
            {
                rec_refreshRemoteValues(child, dev);
            }
        }
        else
        {
            auto addr = Device::address(*n);
            auto& dev = devModel.list().device(addr.device);
            if(!dev.capabilities().canRefresh)
                continue;

            rec_refreshRemoteValues(*n, dev);
        }
    }
}

void NodeUpdateProxy::addLocalNode(
        Device::Node& parent,
        Device::Node&& node)
{
    ISCORE_ASSERT(node.is<Device::AddressSettings>());

    int row = parent.childCount();
    if(deviceExplorer)
    {
        deviceExplorer->addNode(
                    &parent,
                    std::move(node),
                    row);
    }
    else
    {
        parent.emplace(parent.begin() + row,
                       std::move(node));
    }
}


void NodeUpdateProxy::removeLocalNode(const State::Address& addr)
{
    auto parentAddr = addr;
    auto nodeName = parentAddr.path.takeLast();
    auto parentNode = Device::try_getNodeFromAddress(
                           devModel.rootNode(),
                           parentAddr);
    if(parentNode)
    {
        auto it = find_if(*parentNode,
                          [&] (const Device::Node& n) { return n.get<Device::AddressSettings>().name == nodeName; });
        if(it != parentNode->end())
        {
            if(deviceExplorer)
            {
                deviceExplorer->removeNode(it);
            }
            else
            {
                parentNode->erase(it);
            }
        }
    }
}
