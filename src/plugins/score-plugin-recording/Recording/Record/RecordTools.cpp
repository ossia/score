#include "RecordTools.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>

namespace Recording
{
namespace
{
void nodesUnder(Device::Node& parent, std::vector<Device::Node*>& vec)
{
  for(auto& node : parent)
  {
    vec.push_back(&node);
    nodesUnder(node, vec);
  }
}

//! A device whose root is itself a parameter: a camera, a video, a window
bool rootHasParameter(const Device::Node& node, Explorer::DeviceDocumentPlugin& devices)
{
  if(!node.is<Device::DeviceSettings>())
    return false;
  auto* dev = devices.list().findDevice(node.get<Device::DeviceSettings>().name);
  auto* ossia_dev = dev ? dev->getDevice() : nullptr;
  return ossia_dev && ossia_dev->get_root_node().get_parameter();
}
}

std::vector<State::Address> addressesToRecord(Explorer::DeviceExplorerModel& explorer)
{
  auto& devices = explorer.deviceModel();
  auto nodes = explorer.uniqueSelectedNodes(explorer.selectedIndexes());
  std::vector<Device::Node*> all;
  for(auto* node : nodes.parents.empty() ? nodes.messages : nodes.parents)
  {
    all.push_back(node);
    nodesUnder(*node, all);
  }

  std::vector<State::Address> res;
  for(auto* node : all)
  {
    // Audio and texture inputs hold no value, and are recorded too
    const bool address = node->is<Device::AddressSettings>();
    if(!address && !rootHasParameter(*node, devices))
      continue;
    auto addr = Device::address(*node).address;
    if(std::find(res.begin(), res.end(), addr) == res.end())
      res.push_back(std::move(addr));
  }
  return res;
}
}
