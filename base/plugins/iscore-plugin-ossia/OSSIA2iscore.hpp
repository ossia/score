#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
#include <API/Headers/Editor/Value.h>
#include <API/Headers/Editor/Domain.h>

#include <API/Headers/Editor/TimeValue.h>
#include <ProcessInterface/TimeValue.hpp>

// Utility functions to convert from one node to another.
namespace OSSIA
{
namespace convert
{
iscore::IOType ToIOType(OSSIA::Address::AccessMode t);
iscore::ClipMode ToClipMode(OSSIA::Address::BoundingMode b);
iscore::Domain ToDomain(OSSIA::Domain& domain);
iscore::Value ToValue(const OSSIA::Value* val);
iscore::AddressSettings ToAddressSettings(const OSSIA::Node& node);
iscore::Node ToDeviceExplorer(const OSSIA::Node& node);


inline ::TimeValue time(const OSSIA::TimeValue& t)
{
    return t.isInfinite()
            ? ::TimeValue{PositiveInfinity{}}
            : ::TimeValue::fromMsecs(double(t));
}
}
}
