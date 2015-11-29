#pragma once
#include <API/Headers/Editor/TimeValue.h>
#include <API/Headers/Network/Address.h>
#include <Process/TimeValue.hpp>

#include "Device/Address/AddressSettings.hpp"
#include "Device/Address/ClipMode.hpp"
#include "Device/Address/Domain.hpp"
#include "Device/Address/IOType.hpp"
#include "Device/Node/DeviceNode.hpp"
#include "State/Value.hpp"

namespace OSSIA {
class Domain;
class Node;
class Value;
}  // namespace OSSIA

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
