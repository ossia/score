#include "MinuitDevice.hpp"
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/AddressValue.h>

MinuitDevice::MinuitDevice(const DeviceSettings &settings):
    OSSIADevice{settings},
    m_minuitSettings{[&] () {
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    return OSSIA::Minuit{stgs.host.toStdString(),
        stgs.inPort,
        stgs.outPort};
}()}

{
    using namespace OSSIA;

    m_dev = Device::create(m_minuitSettings, settings.name.toStdString());

    Q_ASSERT(m_dev);
}

bool MinuitDevice::canRefresh() const
{
    return true;
}

Node MinuitDevice::refresh()
{
    Node device_node;

    try {
    if(m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        device_node.setDeviceSettings(settings());
        device_node.setAddressSettings(extractAddressSettings(*m_dev.get()));

        // Recurse on the children
        for(const auto& node : m_dev->children())
        {
            device_node.addChild(OssiaToDeviceExplorer(*node.get()));
        }
    }
    }
    catch(std::runtime_error& e)
    {
        qDebug() << "Couldn't load the device:" << e.what();
    }

    device_node.deviceSettings().name = settings().name;

    return device_node;
}
