#include <QChar>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <memory>
#include <vector>

#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
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

  const auto& addr = node.getAddress();

  if (addr)
  {
    addr->requestValue();

    s.name = QString::fromStdString(node.getName());
    s.ioType = addr->getAccessMode();
    s.clipMode = addr->getBoundingMode();
    s.repetitionFilter = addr->getRepetitionFilter();
    s.unit = addr->getUnit();
    s.extendedAttributes = node.getExtendedAttributes();
    s.domain = addr->getDomain();

    try
    {
      s.value = State::fromOSSIAValue(addr->cloneValue());
    }
    catch (...)
    {
      s.value = ToValue(addr->getValueType());
    }
  }
  else
  {
    s.name = QString::fromStdString(node.getName());
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
      auto child_n = ToDeviceExplorer(*ossia_child);
      child_n.setParent(&iscore_node);
      iscore_node.push_back(std::move(child_n));
    }
  }
  return iscore_node;
}

ossia::bounding_mode ToClipMode(ossia::bounding_mode b)
{
  switch (b)
  {
    case ossia::bounding_mode::CLIP:
      return ossia::bounding_mode::CLIP;
    case ossia::bounding_mode::FOLD:
      return ossia::bounding_mode::FOLD;
    case ossia::bounding_mode::FREE:
      return ossia::bounding_mode::FREE;
    case ossia::bounding_mode::WRAP:
      return ossia::bounding_mode::WRAP;
    case ossia::bounding_mode::LOW:
      return ossia::bounding_mode::LOW;
    case ossia::bounding_mode::HIGH:
      return ossia::bounding_mode::HIGH;
    default:
      ISCORE_ABORT;
      return static_cast<ossia::bounding_mode>(-1);
  }
}

State::Address ToAddress(const ossia::net::node_base& node)
{
  State::Address addr;
  const ossia::net::node_base* cur = &node;

  while (auto padre = cur->getParent())
  {
    addr.path.push_front(QString::fromStdString(cur->getName()));
    cur = padre;
  }

  // The last node is the root node "/", which by convention
  // has the same name than the device
  addr.device = QString::fromStdString(cur->getName());
  return addr;
}

State::Value ToValue(ossia::val_type t)
{
  switch (t)
  {
    case ossia::val_type::FLOAT:
      return State::Value::fromValue(float{});
    case ossia::val_type::IMPULSE:
      return State::Value::fromValue(State::impulse{});
    case ossia::val_type::INT:
      return State::Value::fromValue(int{});
    case ossia::val_type::BOOL:
      return State::Value::fromValue(bool{});
    case ossia::val_type::CHAR:
      return State::Value::fromValue(char{});
    case ossia::val_type::STRING:
      return State::Value::fromValue(std::string{});
    case ossia::val_type::TUPLE:
      return State::Value::fromValue(State::tuple_t{});
    case ossia::val_type::VEC2F:
      return State::Value::fromValue(State::vec2f{});
    case ossia::val_type::VEC3F:
      return State::Value::fromValue(State::vec3f{});
    case ossia::val_type::VEC4F:
      return State::Value::fromValue(State::vec4f{});
    case ossia::val_type::DESTINATION:
    default:
      return State::Value{};
  }
}

Device::FullAddressSettings ToFullAddressSettings(
    const State::Address& addr,
    const Device::DeviceList& list)
{
  auto dest = Engine::iscore_to_ossia::makeDestination(list, State::AddressAccessor{addr});
  if(dest)
  {
    return ToFullAddressSettings((*dest).value.get().getNode());
  }

  return {};
}

}
}
