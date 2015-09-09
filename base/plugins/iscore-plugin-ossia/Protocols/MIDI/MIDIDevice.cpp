#include "MIDIDevice.hpp"
#include <API/Headers/Network/Protocol/MIDI.h>
#include <API/Headers/Network/Device.h>
MIDIDevice::MIDIDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;
    auto stgs = settings.deviceSpecificSettings.value<MIDISpecificSettings>();
    auto parameters = OSSIA::MIDI::create();

    m_dev = Device::create(parameters, settings.name.toStdString());
}


void MIDIDevice::updateSettings(const iscore::DeviceSettings&)
{
    ISCORE_TODO;
}
