#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
#include <API/Headers/Editor/Value.h>
#include <API/Headers/Editor/Domain.h>

// Utility functions to convert from one node to another.
namespace OSSIA
{
namespace convert
{
IOType ToIOType(OSSIA::Address::AccessMode t);
ClipMode ToClipMode(OSSIA::Address::BoundingMode b);
iscore::Domain ToDomain(OSSIA::Domain& domain);
QVariant ToVariant(const OSSIA::Value* val);
AddressSettings ToAddressSettings(const OSSIA::Node& node);
iscore::Node* ToDeviceExplorer(const OSSIA::Node& node);
}
}
