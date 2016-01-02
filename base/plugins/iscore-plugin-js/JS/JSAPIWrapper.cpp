#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include "JSAPIWrapper.hpp"

namespace JS
{
QJSValue APIWrapper::value(QJSValue address)
{
    // TODO optional class that moves on first use ?
    auto engine = qjsEngine(this);
    if(!engine)
        return {};

    auto addr_str = address.toString();
    if(State::Address::validateString(addr_str))
    {
        return iscore::convert::JS::value(
                    *engine,
                    devices.updateProxy.refreshRemoteValue(State::Address::fromString(addr_str)));
    }

    return {};
}
}
