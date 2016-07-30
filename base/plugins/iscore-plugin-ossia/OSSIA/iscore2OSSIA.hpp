#pragma once
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/editor/expression/expression_fwd.hpp>
#include <ossia/editor/state/state_element.hpp>

#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>

#include <State/Expression.hpp>
#include <QStringList>
#include <memory>

#include <State/Value.hpp>

#include <iscore_plugin_ossia_export.h>
namespace RecreateOnPlay
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
namespace ossia {
namespace net
{
class address;
class node;
class device;
}
struct Message;
class State;
}  // namespace OSSIA
namespace Device
{
struct FullAddressSettings;
}
namespace State
{
struct Message;
}  // namespace iscore

namespace iscore
{
namespace convert
{
// Gets a node from an address in a device.
// Creates it if necessary.
//// Device-related functions
// OSSIA::net::Node* might be null.
ISCORE_PLUGIN_OSSIA_EXPORT ossia::net::node* findNodeFromPath(
        const QStringList& path,
        ossia::net::device& dev);

// OSSIA::net::Node* won't be null.
ISCORE_PLUGIN_OSSIA_EXPORT ossia::net::node* getNodeFromPath(
        const QStringList& path,
        ossia::net::device& dev);
ISCORE_PLUGIN_OSSIA_EXPORT ossia::net::node* createNodeFromPath(
        const QStringList& path,
        ossia::net::device& dev);

ISCORE_PLUGIN_OSSIA_EXPORT void createOSSIAAddress(
        const Device::FullAddressSettings& settings,
        ossia::net::node& node);
ISCORE_PLUGIN_OSSIA_EXPORT void updateOSSIAAddress(
        const Device::FullAddressSettings& settings,
        ossia::net::address& addr);
ISCORE_PLUGIN_OSSIA_EXPORT void updateOSSIAValue(
        const State::ValueImpl& data,
        ossia::value& val);

ISCORE_PLUGIN_OSSIA_EXPORT ossia::value toOSSIAValue(
        const State::Value&);

//// Other conversions
ISCORE_PLUGIN_OSSIA_EXPORT inline ossia::time_value time(const TimeValue& t)
{
    return t.isInfinite()
            ? ossia::Infinite
            : ossia::time_value{t.msec()};
}

ISCORE_PLUGIN_OSSIA_EXPORT void state(
        ossia::State& ossia_state,
        const Scenario::StateModel& iscore_state,
        const RecreateOnPlay::Context& ctx);
ISCORE_PLUGIN_OSSIA_EXPORT ossia::State state(
        const Scenario::StateModel& iscore_state,
        const RecreateOnPlay::Context& ctx);


ISCORE_PLUGIN_OSSIA_EXPORT optional<ossia::Message> message(
        const State::Message& mess,
        const Device::DeviceList&);

ISCORE_PLUGIN_OSSIA_EXPORT ossia::expression_ptr expression(
        const State::Expression& expr,
        const Device::DeviceList&);


}
}

