#pragma once
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_value.hpp>


#include <Device/Node/DeviceNode.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <QStringList>
#include <State/Expression.hpp>
#include <State/Value.hpp>
#include <memory>
namespace Execution
{
struct Context;
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
struct execution_state;
namespace net
{
class parameter_base;
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
} // namespace score

namespace Engine
{
namespace score_to_ossia
{
// Gets a node from an address in a device.
// Creates it if necessary.
//// Device-related functions
// OSSIA::net::Node* might be null.
ossia::net::node_base*
findAddress(const Device::DeviceList& devices, const State::Address& addr);

optional<ossia::destination> makeDestination(
    const ossia::execution_state& devices, const State::AddressAccessor& addr);

//// Other conversions
inline ossia::time_value
defaultTime(const TimeVal& t)
{
  return t.isInfinite() ? ossia::Infinite
                        : ossia::time_value{int64_t(t.msec() * 1000)};
}

void state(
    ossia::state& ossia_state,
    const Scenario::StateModel& score_state,
    const Execution::Context& ctx);
ossia::state state(
    const Scenario::StateModel& score_state,
    const Execution::Context& ctx);

ossia::expression_ptr
condition_expression(const State::Expression& expr, const ossia::execution_state&);
ossia::expression_ptr
trigger_expression(const State::Expression& expr, const ossia::execution_state&);

ossia::net::node_base* findNode(const ossia::execution_state& st, const State::Address& addr);
}
}
