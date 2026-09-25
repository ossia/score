// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "NodeBasedItemModel.hpp"

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/domain/domain_functions.hpp>
namespace Device
{
NodeBasedItemModel::~NodeBasedItemModel() = default;

static bool is_pattern(QStringView address)
{
  if(address.startsWith(QStringLiteral("//")))
    return true;
  static const auto lit = QStringLiteral("?*[]{}!");
  for(QChar c : address)
  {
    if(std::any_of(lit.begin(), lit.end(), [c](QChar lit) { return lit == c; }))
      return true;
  }

  return false;
}

static bool is_pattern(const State::Address& address)
{
  if(is_pattern(address.device))
    return true;

  for(auto& d : address.path)
    if(is_pattern(d))
      return true;

  return false;
}

Device::FullAddressAccessorSettings makeFullAddressAccessorSettings(
    const State::AddressAccessor& addr, const Device::NodeBasedItemModel& deviceexplorer,
    ossia::value min, ossia::value max, ossia::value val)
{
  auto& newval = addr.address;

  auto newpath = newval.path;
  newpath.prepend(newval.device);

  if(!is_pattern(addr.address))
  {
    // First try to find if there is a matching address
    // in the device explorer
    auto new_n = Device::try_getNodeFromString(deviceexplorer.rootNode(), newpath);
    if(new_n && new_n->is<Device::AddressSettings>())
    {
      return Device::FullAddressAccessorSettings{
          addr, new_n->get<Device::AddressSettings>()};
    }
    else
    {
      // TODO Try also with the OSSIA conversions.
      // But this requires refactoring quite a bit...
    }

    // If there is none, build with some default settings
    Device::FullAddressAccessorSettings s;
    s.address = addr;
    s.value = std::move(val);
    s.domain = ossia::make_domain(std::move(min), std::move(max));
    return s;
  }
  else
  {
    Device::FullAddressAccessorSettings s;
    s.address = addr;
    s.value = std::move(val);
    return s;
  }
}

std::optional<Device::AddressSettings>
addressSettings(const State::Address& addr, const score::DocumentContext& ctx)
{
  if(is_pattern(addr))
    return std::nullopt;
  auto provider
      = ctx.app.interfaces<Device::DeviceModelProviderList>().getBestProvider(ctx);
  if(!provider)
    return std::nullopt;

  if(auto explorer = provider->getNodeModel(ctx))
  {
    auto path = addr.path;
    path.prepend(addr.device);
    auto n = Device::try_getNodeFromString(explorer->rootNode(), path);
    if(n && n->is<Device::AddressSettings>())
      return n->get<Device::AddressSettings>();
  }
  if(auto dev = provider->findDevice(addr.device, ctx))
  {
    auto n = dev->getNodeWithoutChildren(addr);
    if(n.is<Device::AddressSettings>())
      return n.get<Device::AddressSettings>();
  }
  return std::nullopt;
}

Device::FullAddressAccessorSettings makeFullAddressAccessorSettings(
    const State::AddressAccessor& addr, const score::DocumentContext& ctx,
    ossia::value min, ossia::value max, ossia::value val)
{
  if(auto as = addressSettings(addr.address, ctx))
    return Device::FullAddressAccessorSettings{addr, *as};

  Device::FullAddressAccessorSettings s;
  s.address = addr;
  s.value = std::move(val);
  if(!is_pattern(addr.address))
    s.domain = ossia::make_domain(std::move(min), std::move(max));
  return s;
}

Device::FullAddressAccessorSettings
makeFullAddressAccessorSettings(const Device::Node& mess)
{
  if(auto as_ptr = mess.target<Device::AddressSettings>())
  {
    return Device::FullAddressAccessorSettings{Device::address(mess), *as_ptr};
  }
  return {};
}
}
