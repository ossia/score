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
    m_capas.canRefreshTree = true;

    reconnect();
}

bool MIDIDevice::reconnect()
{
    OSSIADevice::disconnect();
    m_dev.reset();

    try {
        auto proto = OSSIA::MIDI::create();
        m_dev = OSSIA::createMIDIDevice(proto);
        m_dev->setName(settings().name.toStdString());
    }
    catch(std::exception& e)
    {
        qDebug() << e.what();
    }

    return connected();
}
}
