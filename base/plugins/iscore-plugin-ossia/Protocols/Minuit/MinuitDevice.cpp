#include "MinuitDevice.hpp"
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Editor/Value.h>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;

MinuitDevice::MinuitDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings},
    m_minuitSettings{[&] () {
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    return OSSIA::Minuit{stgs.host.toStdString(),
        stgs.inPort,
        stgs.outPort};
    }()
    }
{
    using namespace OSSIA;

    m_dev = Device::create(m_minuitSettings, settings.name.toStdString());
}

bool MinuitDevice::canRefresh() const
{
    return true;
}

iscore::Node MinuitDevice::refresh()
{
    iscore::Node device_node;

    if(m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        device_node.setDeviceSettings(settings());
        //device_node.setAddressSettings(ToAddressSettings(*m_dev.get()));

        // Recurse on the children
        for(const auto& node : m_dev->children())
        {
            device_node.addChild(ToDeviceExplorer(*node.get()));
        }
    }

    device_node.deviceSettings().name = settings().name;

    return device_node;
}

iscore::Value MinuitDevice::refresh(const iscore::Address & address)
{
    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
    auto addr = node->getAddress();
    addr->updateValue();
    return ToValue(addr->getValue());
}
