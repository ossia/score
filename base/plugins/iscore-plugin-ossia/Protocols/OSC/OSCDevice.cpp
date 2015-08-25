#include "OSCDevice.hpp"
#include <API/Headers/Network/Protocol/OSC.h>
#include <API/Headers/Network/Device.h>
OSCDevice::OSCDevice(const iscore::DeviceSettings &stngs):
    OSSIADevice{stngs}
{
    using namespace OSSIA;

    auto settings = stngs.deviceSpecificSettings.value<OSCSpecificSettings>();
    OSSIA::OSC oscDeviceParameter{settings.host.toStdString(), settings.inputPort, settings.outputPort};
    m_dev = OSSIA::Device::create(oscDeviceParameter);
}
