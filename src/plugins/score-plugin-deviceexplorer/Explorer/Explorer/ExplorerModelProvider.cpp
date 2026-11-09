#include "ExplorerModelProvider.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QPointer>

#include <algorithm>
#include <vector>

namespace Explorer
{
namespace
{
void collectAddresses(const Device::Node& node, std::vector<State::Address>& out)
{
  for(const auto& child : node)
  {
    if(child.is<Device::AddressSettings>())
      out.push_back(Device::address(child).address);
    collectAddresses(child, out);
  }
}

//! Listens to the addresses of the devices that were not listened to, and
//! stops listening to them, and to them alone, once destroyed.
struct ListenToAll
{
  struct Listened
  {
    // A device may be removed, or its document closed, before the end
    QPointer<Device::DeviceInterface> device;
    std::vector<State::Address> added;
  };
  std::vector<Listened> devices;

  ~ListenToAll()
  {
    for(auto& [device, added] : devices)
      if(device)
        for(const auto& addr : added)
          device->setListening(addr, false);
  }
};
}

std::shared_ptr<void>
ModelProvider::listenToAllAddresses(const score::DocumentContext& ctx) const noexcept
{
  auto plug = ctx.findPlugin<DeviceDocumentPlugin>();
  if(!plug)
    return {};

  auto res = std::make_shared<ListenToAll>();
  for(auto device : plug->list().devices())
  {
    if(!device || !device->capabilities().canListen)
      continue;

    const auto& name = device->settings().name;
    auto& root = plug->rootNode();
    auto node = std::find_if(root.begin(), root.end(), [&](const Device::Node& n) {
      return n.displayName() == name;
    });
    if(node == root.end())
      continue;

    std::vector<State::Address> all;
    collectAddresses(*node, all);

    const auto before = device->listeningRequests();
    const ossia::hash_set<State::Address> listened(before.begin(), before.end());

    ListenToAll::Listened l{device, {}};
    for(auto& addr : all)
    {
      if(listened.count(addr) > 0)
        continue;
      device->setListening(addr, true, false);
      l.added.push_back(std::move(addr));
    }
    if(!l.added.empty())
      res->devices.push_back(std::move(l));
  }
  return res;
}
}
