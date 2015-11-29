#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Protocol/MIDI.h>
#include <QString>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "MIDIDevice.hpp"

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
