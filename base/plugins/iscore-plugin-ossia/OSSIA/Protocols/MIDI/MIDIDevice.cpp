#include <ossia/network/base/device.hpp>
#include <ossia/network/midi/midi.hpp>
#include <QString>
#include <memory>
#include <OSSIA/Protocols/MIDI/MIDISpecificSettings.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include "MIDIDevice.hpp"
#include <OSSIA/OSSIA2iscore.hpp>

namespace Ossia
{
namespace Protocols
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
        auto proto = std::make_unique<OSSIA::net::MIDI>();
        proto->setInfo(OSSIA::net::MidiInfo(
                           static_cast<OSSIA::net::MidiInfo::Type>(set.io),
                           set.endpoint.toStdString(),
                           set.port));
        m_dev = std::make_unique<OSSIA::net::MIDIDevice>(std::move(proto));
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
        removeListening_impl(m_dev->getRootNode(), State::Address{m_settings.name, {}});
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
        auto& children = m_dev->getRootNode().children();
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
}
