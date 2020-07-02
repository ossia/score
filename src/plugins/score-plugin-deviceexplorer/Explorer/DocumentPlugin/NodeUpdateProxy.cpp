// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "NodeUpdateProxy.hpp"

#include "DeviceDocumentPlugin.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <State/Address.hpp>

#include <score/model/tree/TreeNode.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QDebug>

#include <vector>

namespace Explorer
{
NodeUpdateProxy::NodeUpdateProxy(DeviceDocumentPlugin& root) : devModel{root} { }

void NodeUpdateProxy::addDevice(const Device::Node& node)
{
  devModel.explorer().addDevice(devModel.createDeviceFromNode(node));
}

void NodeUpdateProxy::loadDevice(const Device::Node& node)
{
  auto n = devModel.loadDeviceFromNode(node);
  if (n)
  {
    devModel.explorer().addDevice(std::move(*n));
  }
  else
  {
    devModel.explorer().addDevice(node);
  }
}

void NodeUpdateProxy::updateDevice(const QString& name, const Device::DeviceSettings& dev)
{
  devModel.list().device(name).updateSettings(dev);
  devModel.explorer().updateDevice(name, dev);
}

void NodeUpdateProxy::removeDevice(const Device::DeviceSettings& dev)
{
  const auto& rootNode = devModel.rootNode();
  auto it = ossia::find_if(rootNode, [&](const Device::Node& val) {
    return val.is<Device::DeviceSettings>() && val.get<Device::DeviceSettings>().name == dev.name;
  });
  SCORE_ASSERT(it != rootNode.end());
  auto dev_i = devModel.list().findDevice(dev.name);
  SCORE_ASSERT(dev_i);
  devModel.setupConnections(*dev_i, false);

  devModel.explorer().removeNode(it);

  devModel.list().removeDevice(dev.name);
}

void NodeUpdateProxy::addAddress(
    const Device::NodePath& parentPath,
    const Device::AddressSettings& settings,
    int row)
{
  auto parentnode = parentPath.toNode(&devModel.rootNode());
  if (!parentnode)
    return;

  // Add in the device impl
  // Get the device node :
  // TODO row isn't managed here.
  const Device::Node& dev_node = devModel.rootNode().childAt(parentPath.at(0));
  SCORE_ASSERT(dev_node.template is<Device::DeviceSettings>());

  // Make a full path
  Device::FullAddressSettings full
      = Device::FullAddressSettings::make<Device::FullAddressSettings::as_parent>(
          settings, Device::address(*parentnode));

  // Add in the device implementation
  devModel.list().device(dev_node.template get<Device::DeviceSettings>().name).addAddress(full);

  // Add in the device explorer
  addLocalAddress(*parentnode, settings, row);
}

void NodeUpdateProxy::addAddress(const Device::FullAddressSettings& full)
{
  // Add in the device implementation
  auto& dev = devModel.list().device(full.address.device);
  bool learning = dev.isLearning();
  dev.setLearning(true);
  dev.addAddress(full);
  dev.setLearning(learning);
  // Add in the device explorer
  // addLocalAddress(*parentnode, settings, row);
}

void NodeUpdateProxy::rec_addNode(Device::NodePath parentPath, const Device::Node& n, int row)
{
  addAddress(parentPath, n.template get<Device::AddressSettings>(), row);

  parentPath.append(row);

  int r = 0;
  for (const auto& child : n.children())
  {
    rec_addNode(parentPath, child, r++);
  }
}

void NodeUpdateProxy::addNode(
    const Device::NodePath& parentPath,
    const Device::Node& node,
    int row)
{
  SCORE_ASSERT(node.template is<Device::AddressSettings>());

  rec_addNode(parentPath, node, row);
}

void NodeUpdateProxy::updateAddress(
    const Device::NodePath& nodePath,
    const Device::AddressSettings& settings)
{
  auto node = nodePath.toNode(&devModel.rootNode());
  if (!node)
    return;

  const auto addr = Device::address(*node);
  // Make a full path
  Device::FullAddressSettings full
      = Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(settings, addr);
  full.address.path.last() = settings.name;

  // Update in the device implementation
  devModel.list().device(addr.address.device).updateAddress(addr.address, full);

  // Update in the device explorer
  devModel.explorer().updateAddress(node, settings);
}

void NodeUpdateProxy::removeNode(
    const Device::NodePath& parentPath,
    const Device::AddressSettings& settings)
{
  Device::Node* parentnode = parentPath.toNode(&devModel.rootNode());
  if (!parentnode)
    return;

  auto addr = Device::address(*parentnode).address;
  addr.path.append(settings.name);

  // Remove from the device implementation
  const auto& dev_node = devModel.rootNode().childAt(parentPath.at(0));
  devModel.list().device(dev_node.template get<Device::DeviceSettings>().name).removeNode(addr);

  // Remove from the device explorer
  auto it = std::find_if(parentnode->begin(), parentnode->end(), [&](const Device::Node& n) {
    return n.get<Device::AddressSettings>().name == settings.name;
  });
  SCORE_ASSERT(it != parentnode->end());

  devModel.explorer().removeNode(it);
}

void NodeUpdateProxy::addLocalAddress(
    Device::Node& parentnode,
    const Device::AddressSettings& settings,
    int row)
{
  devModel.explorer().addAddress(&parentnode, settings, row);
}

void NodeUpdateProxy::updateLocalValue(const State::AddressAccessor& addr, const ossia::value& v)
{
  auto n = Device::try_getNodeFromAddress(devModel.rootNode(), addr.address);
  if (!n)
    return;

  if (!n->template is<Device::AddressSettings>())
  {
    qDebug() << "Updating invalid node";
    return;
  }

  devModel.explorer().updateValue(n, addr, v);
}

void NodeUpdateProxy::updateLocalSettings(
    const State::Address& addr,
    const Device::AddressSettings& set,
    Device::DeviceInterface& newdev)
{
  auto n = Device::try_getNodeFromAddress(devModel.rootNode(), addr);
  if (!n)
  {
    // FIXME A subtle bug is introduced if we want to add the root node...
    if (addr.path.size() > 0)
    {
      auto parentAddr = addr;
      parentAddr.path.removeLast();

      Device::Node* parent = Device::try_getNodeFromAddress(devModel.rootNode(), parentAddr);
      if (parent)
      {
        const auto& last = addr.path.back();
        auto it = ossia::find_if(*parent, [&](const auto& n) { return n.displayName() == last; });
        if (it == parent->cend())
        {
          addLocalNode(*parent, newdev.getNode(addr));
        }
        else
        {
          // TODO update the node with the new information
        }
      }
    }
    return;
  }

  if (!n->template is<Device::AddressSettings>())
  {
    qDebug() << "Updating invalid node";
    return;
  }

  devModel.explorer().updateAddress(n, set);
}

void NodeUpdateProxy::updateRemoteValue(const State::Address& addr, const ossia::value& val)
{
  // TODO add these checks everywhere.
  if (auto dev = devModel.list().findDevice(addr.device))
  {
    // Update in the device implementation
    dev->sendMessage(addr, val);
  }
}

ossia::value NodeUpdateProxy::refreshRemoteValue(const State::Address& addr) const
{
  // TODO here and in the following function, we should still update
  // the device explorer.
  auto dev_p = devModel.list().findDevice(addr.device);
  if (!dev_p)
    return {};

  auto& dev = *dev_p;

  auto& n = Device::getNodeFromAddress(devModel.rootNode(), addr)
                .template get<Device::AddressSettings>();
  if (dev.capabilities().canRefreshValue)
  {
    if (auto val = dev.refresh(addr))
    {
      n.value = *val;
    }
  }

  return n.value;
}

std::optional<ossia::value> NodeUpdateProxy::try_refreshRemoteValue(const State::Address& addr) const
{
  // TODO here and in the following function, we should still update
  // the device explorer.
  auto dev_p = devModel.list().findDevice(addr.device);
  if (!dev_p)
    return {};

  auto& dev = *dev_p;

  auto node = Device::try_getNodeFromAddress(devModel.rootNode(), addr);
  if (!node)
    return {};

  auto& n = node->template get<Device::AddressSettings>();
  if (dev.capabilities().canRefreshValue)
  {
    if (auto val = dev.refresh(addr))
    {
      n.value = *val;
    }
  }

  return n.value;
}

static void rec_refreshRemoteValues(Device::Node& n, Device::DeviceInterface& dev)
{
  // OPTIMIZEME
  auto val = dev.refresh(Device::address(n).address);
  if (val)
    n.template get<Device::AddressSettings>().value = *val;

  for (auto& child : n)
  {
    rec_refreshRemoteValues(child, dev);
  }
}

void NodeUpdateProxy::refreshRemoteValues(const Device::NodeList& nodes)
{
  // For each node, get its device.
  for (auto n : nodes)
  {
    if (n->template is<Device::DeviceSettings>())
    {
      auto dev_name = n->template get<Device::DeviceSettings>().name;
      auto& dev = devModel.list().device(dev_name);
      if (!dev.capabilities().canRefreshValue)
        continue;

      for (auto& child : *n)
      {
        rec_refreshRemoteValues(child, dev);
      }
    }
    else
    {
      auto addr = Device::address(*n);
      auto& dev = devModel.list().device(addr.address.device);
      if (!dev.capabilities().canRefreshValue)
        continue;

      rec_refreshRemoteValues(*n, dev);
    }
  }
}

void NodeUpdateProxy::addLocalNode(Device::Node& parent, Device::Node&& node)
{
  if (node.is<Device::AddressSettings>())
  {
    int row = parent.childCount();
    devModel.explorer().addNode(&parent, std::move(node), row);
  }
}

void NodeUpdateProxy::removeLocalNode(const State::Address& addr)
{
  auto parentAddr = addr;
  auto nodeName = parentAddr.path.takeLast();
  auto parentNode = Device::try_getNodeFromAddress(devModel.rootNode(), parentAddr);
  if (parentNode)
  {
    auto it = ossia::find_if(*parentNode, [&](const Device::Node& n) {
      return n.get<Device::AddressSettings>().name == nodeName;
    });
    if (it != parentNode->end())
    {
      devModel.explorer().removeNode(it);
    }
  }
}
}
