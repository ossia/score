#include "OSCDevice.hpp"
OSCDevice::OSCDevice(const DeviceSettings &stngs):
    OSSIADevice{stngs}
{
    //auto settings = stngs.deviceSpecificSettings.value<OSCSpecificSettings>();
    //OSSIA::OSC oscDeviceParameter{settings.host.toStdString(), settings.inputPort, settings.outputPort};
    //m_device = OSSIA::Device::create(oscDeviceParameter);
}
