#include <ossia/network/v1/Device.hpp>
#include <ossia/network/v1/Protocol/MIDI.hpp>
#include <QString>
#include <memory>
#include <OSSIA/Protocols/MIDI/MIDISpecificSettings.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include "MIDIDevice.hpp"
#include <OSSIA/OSSIA2iscore.hpp>

namespace Ossia
{
MIDIDevice::MIDIDevice(const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    using namespace OSSIA;
    m_capas.canRefreshTree = true;
    m_capas.canSerialize = false;

    reconnect();
}

bool MIDIDevice::reconnect()
{
    disconnect();
    m_dev.reset();

    MIDISpecificSettings set = settings().deviceSpecificSettings.value<MIDISpecificSettings>();
    try {
        auto proto = OSSIA::MIDI::create();
        proto->setInfo(OSSIA::MidiInfo(
                           static_cast<OSSIA::MidiInfo::Type>(set.io),
                           set.endpoint.toStdString(),
                           set.port));
        m_dev = OSSIA::createMIDIDevice(proto);
        m_dev->setName(settings().name.toStdString());
        m_dev->updateNamespace();
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

Device::Node MIDIDevice::refresh()
{
    Device::Node device_node{settings(), nullptr};

    if(!connected())
    {
        return device_node;
    }
    else
    {
        auto& children = m_dev->children();
        device_node.reserve(children.size());
        for(const auto& node : children)
        {
            device_node.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
        }
    }

    device_node.get<Device::DeviceSettings>().name = settings().name;
    return device_node;
}

}
