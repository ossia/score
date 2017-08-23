// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QChar>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <memory>
#include <vector>

#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/domain/domain.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Engine
{
namespace ossia_to_iscore
{
Device::AddressSettings ToAddressSettings(const ossia::net::node_base& node)
{
  Device::AddressSettings s;

  const auto& addr = node.get_parameter();

  if (addr)
  {
    addr->request_value();

    s.name = QString::fromStdString(node.get_name());
    s.ioType = addr->get_access();
    s.clipMode = addr->get_bounding();
    s.repetitionFilter = addr->get_repetition_filter();
    s.unit = addr->get_unit();
    s.extendedAttributes = node.get_extended_attributes();
    s.domain = addr->get_domain();

    try
    {
      s.value = addr->value();
    }
    catch (...)
    {
      s.value = ossia::init_value(addr->get_value_type());
    }
  }
  else
  {
    s.name = QString::fromStdString(node.get_name());
  }
  return s;
}

Device::FullAddressSettings
ToFullAddressSettings(const ossia::net::node_base& node)
{
  Device::FullAddressSettings set;
  static_cast<Device::AddressSettingsCommon&>(set) = ToAddressSettings(node);
  set.address = ToAddress(node);
  return set;
}

Device::Node ToDeviceExplorer(const ossia::net::node_base& ossia_node)
{

  Device::Node iscore_node{ToAddressSettings(ossia_node), nullptr};
  {
    const auto& cld = ossia_node.children();
    iscore_node.reserve(cld.size());

    // 2. Recurse on the children
    for (const auto& ossia_child : cld)
    {
      if(!ossia::net::get_hidden(*ossia_child) && !ossia::net::get_zombie(*ossia_child))
      {
        auto child_n = ToDeviceExplorer(*ossia_child);
        child_n.setParent(&iscore_node);
        iscore_node.push_back(std::move(child_n));
      }
    }
  }
  return iscore_node;
}

void ToAddress_rec(State::Address& addr, const ossia::net::node_base* cur, int N)
{
  if(auto padre = cur->get_parent())
  {
    ToAddress_rec(addr, padre, N + 1);
    addr.path.push_back(QString::fromStdString(cur->get_name()));
  }
  else
  {
    // The last node is the root node "/", which by convention
    // has the same name than the device
    addr.path.reserve(N);
    addr.device = QString::fromStdString(cur->get_name());
  }
}

State::Address ToAddress(const ossia::net::node_base& node)
{
  State::Address addr;
  const ossia::net::node_base* cur = &node;

  ToAddress_rec(addr, cur, 1);
  return addr;
}

Device::FullAddressSettings ToFullAddressSettings(
    const State::Address& addr,
    const Device::DeviceList& list)
{
  auto dest = Engine::iscore_to_ossia::makeDestination(list, State::AddressAccessor{addr});
  if(dest)
  {
    return ToFullAddressSettings((*dest).address().get_node());
  }

  return {};
}

}
}
