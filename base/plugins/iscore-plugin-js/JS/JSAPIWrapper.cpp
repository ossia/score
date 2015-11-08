#include "JSAPIWrapper.hpp"
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

QJSValue JSAPIWrapper::value(QJSValue address)
{
    // TODO optional class that moves on first use ?
    auto engine = qjsEngine(this);
    if(!engine)
        return {};

    auto addr_str = address.toString();
    if(iscore::Address::validateString(addr_str))
    {
        return iscore::convert::js::value(
                    *engine,
                    devices.updateProxy.refreshRemoteValue(iscore::Address::fromString(addr_str)));
    }

    return {};
}
