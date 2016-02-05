#pragma once
#include <Editor/TimeValue.h>
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <State/Expression.hpp>
#include <QStringList>
#include <memory>

#include <State/Value.hpp>
#include <iscore_plugin_ossia_export.h>
namespace Scenario
{
class StateModel;
}
namespace Device
{
class DeviceList;
}
namespace OSSIA {
class Address;
class Device;
class Expression;
class Message;
class Node;
class State;
class Value;
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
// OSSIA::Node* might be null.
ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Node* findNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::Node> findNodeFromPath(
        const QStringList& path,
        std::shared_ptr<OSSIA::Device> dev);

// OSSIA::Node* won't be null.
ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Node* getNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Node* createNodeFromPath(
        const QStringList& path,
        OSSIA::Device* dev);

ISCORE_PLUGIN_OSSIA_EXPORT void createOSSIAAddress(
        const Device::FullAddressSettings& settings,
        OSSIA::Node* node);
ISCORE_PLUGIN_OSSIA_EXPORT void updateOSSIAAddress(
        const Device::FullAddressSettings& settings,
        const std::shared_ptr<OSSIA::Address>& addr);
ISCORE_PLUGIN_OSSIA_EXPORT void removeOSSIAAddress(
        OSSIA::Node*); // Keeps the Node.
ISCORE_PLUGIN_OSSIA_EXPORT void updateOSSIAValue(
        const State::ValueImpl& data,
        OSSIA::Value& val);

ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Value* toOSSIAValue(
        const State::Value&);

//// Other conversions
ISCORE_PLUGIN_OSSIA_EXPORT inline OSSIA::TimeValue time(const TimeValue& t)
{
    return t.isInfinite()
            ? OSSIA::Infinite
            : OSSIA::TimeValue{t.msec()};
}

ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::State> state(
        std::shared_ptr<OSSIA::State> ossia_state,
        const Scenario::StateModel& iscore_state,
        const Device::DeviceList& deviceList);
ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::State> state(
        const Scenario::StateModel& iscore_state,
        const Device::DeviceList& deviceList);


ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::Message> message(
        const State::Message& mess,
        const Device::DeviceList&);

ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::Expression> expression(
        const State::Expression& expr,
        const Device::DeviceList&);


}
}

