#include "MIDIDevice.hpp"
#include <API/Headers/Network/Protocol/MIDI.h>
#include <API/Headers/Network/Device.h>
MIDIDevice::MIDIDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;

    reconnect();
}

bool MIDIDevice::reconnect()
{
    m_dev.reset();

    try {
        m_dev = OSSIA::Device::create(OSSIA::MIDI::create(), settings().name.toStdString());
    }
    catch(...)
    {
    }

    return connected();
}
