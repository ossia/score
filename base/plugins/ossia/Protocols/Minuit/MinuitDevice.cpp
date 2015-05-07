#include "MinuitDevice.hpp"
MinuitDevice::MinuitDevice(const DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    Minuit minuitDeviceParameters{settings.name.toStdString(),
                                  stgs.host.toStdString(),
                                  stgs.port};

    m_dev = Device::create(minuitDeviceParameters, settings.name.toStdString());
}
