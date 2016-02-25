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
        return iscore::convert::JS::value(
                    m_engine,
                    devices.updateProxy.refreshRemoteValue(State::Address::fromString(addr_str)));
    }

    return {};
}
}
