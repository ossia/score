#include <Network/Device.h>
#include <Network/Protocol/MIDI.h>
#include <QString>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "MIDIDevice.hpp"

namespace Ossia
{
MIDIDevice::MIDIDevice(const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;

    reconnect();
}

bool MIDIDevice::reconnect()
{
    OSSIADevice::disconnect();
    m_dev.reset();

    try {
        m_dev = OSSIA::Device::create(OSSIA::MIDI::create(), settings().name.toStdString());
    }
    catch(...)
    {
    }

    return connected();
}
}
