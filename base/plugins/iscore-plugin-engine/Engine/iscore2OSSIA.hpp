#pragma once
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <QStringList>
#include <State/Expression.hpp>
#include <memory>

#include <State/Value.hpp>

#include <iscore_plugin_engine_export.h>
namespace Engine
{
namespace Execution
{
struct Context;
}
}
namespace Scenario
{
class StateModel;
}
namespace Device
{
class DeviceList;
}
namespace ossia
{
namespace net
{
class address_base;
class node_base;
class device_base;
}
struct message;
class state;
} // namespace OSSIA
namespace Device
{
struct FullAddressSettings;
}
namespace State
{
struct Message;
} // namespace iscore

namespace Engine
{
namespace iscore_to_ossia
{
// Gets a node from an address in a device.
// Creates it if necessary.
//// Device-related functions
// OSSIA::net::Node* might be null.
ISCORE_PLUGIN_ENGINE_EXPORT ossia::net::node_base*
findNodeFromPath(const QStringList& path, ossia::net::device_base& dev);
ISCORE_PLUGIN_ENGINE_EXPORT ossia::net::node_base*
findNodeFromPath(const Device::Node& path, ossia::net::device_base& dev);

ISCORE_PLUGIN_ENGINE_EXPORT ossia::net::address_base*
findAddress(const Device::DeviceList& devices, const State::Address& addr);

ISCORE_PLUGIN_ENGINE_EXPORT optional<ossia::Destination> makeDestination(
    const Device::DeviceList& devices, const State::AddressAccessor& addr);

// OSSIA::net::Node* won't be null.
ISCORE_PLUGIN_ENGINE_EXPORT ossia::net::node_base*
getNodeFromPath(const QStringList& path, ossia::net::device_base& dev);
ISCORE_PLUGIN_ENGINE_EXPORT ossia::net::node_base*
createNodeFromPath(const QStringList& path, ossia::net::device_base& dev);

ISCORE_PLUGIN_ENGINE_EXPORT void createOSSIAAddress(
    const Device::FullAddressSettings& settings, ossia::net::node_base& node);
ISCORE_PLUGIN_ENGINE_EXPORT void updateOSSIAAddress(
    const Device::FullAddressSettings& settings,
    ossia::net::address_base& addr);
ISCORE_PLUGIN_ENGINE_EXPORT void
updateOSSIAValue(const ossia::value& data, ossia::value& val);

//// Other conversions
ISCORE_PLUGIN_ENGINE_EXPORT inline ossia::time_value defaultTime(const TimeVal& t)
{
  return t.isInfinite() ? ossia::Infinite : ossia::time_value{t.msec() * 1000.};
}

ISCORE_PLUGIN_ENGINE_EXPORT void state(
    ossia::state& ossia_state,
    const Scenario::StateModel& iscore_state,
    const Engine::Execution::Context& ctx);
ISCORE_PLUGIN_ENGINE_EXPORT ossia::state state(
    const Scenario::StateModel& iscore_state,
    const Engine::Execution::Context& ctx);

ISCORE_PLUGIN_ENGINE_EXPORT
ossia::net::address_base* address(
    const State::Address& addr,
    const Device::DeviceList& deviceList);

ISCORE_PLUGIN_ENGINE_EXPORT optional<ossia::message>
message(const State::Message& mess, const Device::DeviceList&);

ISCORE_PLUGIN_ENGINE_EXPORT ossia::expression_ptr
condition_expression(const State::Expression& expr, const Device::DeviceList&);
ISCORE_PLUGIN_ENGINE_EXPORT ossia::expression_ptr
trigger_expression(const State::Expression& expr, const Device::DeviceList&);
}
}
