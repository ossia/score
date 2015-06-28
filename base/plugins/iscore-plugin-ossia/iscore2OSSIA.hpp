#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
#include <API/Headers/Editor/Value.h>
#include <API/Headers/Editor/Domain.h>

#include <API/Headers/Editor/TimeValue.h>
#include <ProcessInterface/TimeValue.hpp>


namespace iscore
{
namespace convert
{
// Gets a node from an address in a device.
// Creates it if necessary.
OSSIA::Node* getNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
OSSIA::Node* createNodeFromPath(
        const QStringList& path,
        OSSIA::Device* dev);

void createOSSIAAddress(
        const FullAddressSettings& settings,
        OSSIA::Node* node);
void updateOSSIAAddress(
        const FullAddressSettings& settings,
        const std::shared_ptr<OSSIA::Address>& addr);
void updateOSSIAValue(
        const QVariant& data,
        OSSIA::Value& val);

OSSIA::Value* toValue(
        const QVariant&);

inline OSSIA::TimeValue time(const TimeValue& t)
{
    return t.isInfinite()
            ? OSSIA::TimeValue{t.isInfinite()}
            : OSSIA::TimeValue{t.msec()};
}

}
}

