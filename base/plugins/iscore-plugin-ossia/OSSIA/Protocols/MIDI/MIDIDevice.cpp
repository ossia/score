#include <Network/Device.h>
#include <Network/Protocol/MIDI.h>
#include <QString>
#include <memory>
#include <OSSIA/Protocols/MIDI/MIDISpecificSettings.hpp>

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

    MIDISpecificSettings set = settings().deviceSpecificSettings.value<MIDISpecificSettings>();
    try {
        auto proto = OSSIA::MIDI::create();
        m_dev = OSSIA::createMIDIDevice(proto);
        m_dev->setName(settings().name.toStdString());
        proto->setInfo(OSSIA::MidiInfo(
                           static_cast<OSSIA::MidiInfo::Type>(set.io),
                           set.endpoint.toStdString(),
                           set.port));
    }
    catch(std::exception& e)
    {
        qDebug() << e.what();
    }

    return connected();
}

void MIDIDevice::disconnect()
{
    if(connected())
    {
        removeListening_impl(*m_dev.get(), State::Address{m_settings.name, {}});
    }

    m_callbacks.clear();
    m_dev.reset();
}

}
