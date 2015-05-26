#include "MIDIDevice.hpp"

MIDIDevice::MIDIDevice(const DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;
    auto stgs = settings.deviceSpecificSettings.value<MIDISpecificSettings>();
    Midi parameters;

    m_dev = Device::create(parameters, settings.name.toStdString());
}
