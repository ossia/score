#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
#include <API/Headers/Editor/Value.h>
#include <API/Headers/Editor/Domain.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>

#include <API/Headers/Editor/TimeValue.h>
#include <ProcessInterface/TimeValue.hpp>
#include <State/Message.hpp>
#include <State/Expression.hpp>

#include <DeviceExplorer/Protocol/DeviceList.hpp>
#include <ProcessInterface/State/MessageNode.hpp>
namespace iscore
{
namespace convert
{
// Gets a node from an address in a device.
// Creates it if necessary.
//// Device-related functions
// OSSIA::Node* might be null.
OSSIA::Node* findNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
std::shared_ptr<OSSIA::Node> findNodeFromPath(
        const QStringList& path,
        std::shared_ptr<OSSIA::Device> dev);

// OSSIA::Node* won't be null.
OSSIA::Node* getNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
OSSIA::Node* createNodeFromPath(
        const QStringList& path,
        OSSIA::Device* dev);

void createOSSIAAddress(
        const iscore::FullAddressSettings& settings,
        OSSIA::Node* node);
void updateOSSIAAddress(
        const iscore::FullAddressSettings& settings,
        const std::shared_ptr<OSSIA::Address>& addr);
void removeOSSIAAddress(
        OSSIA::Node*); // Keeps the Node.
void updateOSSIAValue(
        const QVariant& data,
        OSSIA::Value& val);

OSSIA::Value* toValue(
        const iscore::Value&);

//// Other conversions
inline OSSIA::TimeValue time(const TimeValue& t)
{
    return t.isInfinite()
            ? OSSIA::TimeValue{t.isInfinite()}
            : OSSIA::TimeValue{t.msec()};
}

std::shared_ptr<OSSIA::State> state(std::shared_ptr<OSSIA::State> ossia_state,
        const MessageNode& iscore_state,
        const DeviceList& deviceList);


std::shared_ptr<OSSIA::Message> message(
        const iscore::Message& mess,
        const DeviceList&);

std::shared_ptr<OSSIA::Expression> expression(
        const iscore::Expression& expr,
        const DeviceList&);
}
}

