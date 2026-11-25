#include "RecordTools.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/audio/audio_parameter.hpp>

#include <ossia/protocols/midi/midi_device.hpp>

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

//! A MIDI device: recorded as a whole, as the messages it gives
bool isMidiDevice(const Device::Node& node, Explorer::DeviceDocumentPlugin& devices)
{
  if(!node.is<Device::DeviceSettings>())
    return false;
  auto* dev = devices.list().findDevice(node.get<Device::DeviceSettings>().name);
  auto* ossia_dev = dev ? dev->getDevice() : nullptr;
  return dynamic_cast<ossia::net::midi::midi_device*>(ossia_dev);
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
    // A MIDI device's notes and controllers are in what it gives as a whole
    if(!isMidiDevice(*node, devices))
      nodesUnder(*node, all);
  }

  std::vector<State::Address> res;
  for(auto* node : all)
  {
    // Audio and texture inputs hold no value, and are recorded too
    const bool address = node->is<Device::AddressSettings>();
    if(!address && !rootHasParameter(*node, devices) && !isMidiDevice(*node, devices))
      continue;
    auto addr = Device::address(*node).address;
    if(std::find(res.begin(), res.end(), addr) == res.end())
      res.push_back(std::move(addr));
  }
  return res;
}

AddressKinds addressKinds(
    Explorer::DeviceDocumentPlugin& devices, const std::vector<State::Address>& addresses)
{
  AddressKinds k;
  for(const auto& a : addresses)
  {
    auto* dev = devices.list().findDevice(a.device);
    auto* odev = dev ? dev->getDevice() : nullptr;
    if(!odev)
      continue;
    if(a.path.isEmpty() && dynamic_cast<ossia::net::midi::midi_device*>(odev))
    {
      k.midi = true;
      continue;
    }
    auto* node = a.path.isEmpty() ? &odev->get_root_node()
                                  : ossia::net::find_node(
                                      odev->get_root_node(),
                                      a.toString_unsafe().section(':', 1).toStdString());
    auto* p = node ? node->get_parameter() : nullptr;
    // Sounds, and a device whose root is its image (a camera)
    if(p && (dynamic_cast<ossia::audio_parameter*>(p) || a.path.isEmpty()))
      k.streams = true;
    else
      k.values = true;
  }
  return k;
}
}
