#include "LearnRollback.hpp"

#include <Device/Node/DeviceNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

namespace Explorer
{
namespace
{
//! The shallowest node of \p now that \p before did not have.
const Device::Node* firstAdded(const Device::Node& before, const Device::Node& now)
{
  for(const auto& child : now)
    if(!findChildNode(before, child.displayName()))
      return &child;

  for(const auto& child : now)
    if(const auto* old = findChildNode(before, child.displayName()))
      if(const auto* deeper = firstAdded(*old, child))
        return deeper;

  return nullptr;
}

int countNodes(const Device::Node& n)
{
  int c = 1;
  for(const auto& child : n)
    c += countNodes(child);
  return c;
}

Device::Node* deviceNode(DeviceDocumentPlugin& plug, const QString& name)
{
  for(auto& n : plug.rootNode())
    if(n.is<Device::DeviceSettings>() && n.get<Device::DeviceSettings>().name == name)
      return &n;
  return nullptr;
}
}

void rollbackLearnedNodes(
    DeviceDocumentPlugin& plug, const QString& deviceName, const Device::Node& before)
{
  Device::Node* live = deviceNode(plug, deviceName);
  if(!live)
    return;

  // A learn adds a handful of addresses. The bound is only here so that a
  // protocol which declines to remove one cannot spin forever.
  for(int guard = 0, n = countNodes(*live); guard < n; guard++)
  {
    const Device::Node* added = firstAdded(before, *live);
    if(!added)
      return;

    Device::Node* parent = added->parent();
    if(!parent)
      return;

    const auto settings = added->get<Device::AddressSettings>();
    const int was = countNodes(*live);

    plug.updateProxy.removeNode(Device::NodePath{*parent}, settings);

    // The protocol refused: stop rather than ask again for the same node.
    if(countNodes(*live) >= was)
      return;
  }
}
}
