#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include "JSAPIWrapper.hpp"

namespace JS
{
QJSValue APIWrapper::value(QJSValue address)
{
    // OPTIMIZEME : have State::Address::fromString return a optional<Address> to have a single check.
    auto addr_str = address.toString();
    if(State::Address::validateString(addr_str))
    {
        auto val = devices.updateProxy.try_refreshRemoteValue(State::Address::fromString(addr_str));
        if(val)
        {
            return JS::convert::value(
                        m_engine,
                        *std::move(val));
        }
    }

    return {};
}
}
