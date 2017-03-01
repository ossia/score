#include <ossia/network/base/device.hpp>
#include <ossia/network/midi/midi.hpp>
#include <Engine/Protocols/MIDI/MIDISpecificSettings.hpp>
#include <QString>
#include <memory>

#include "MIDIDevice.hpp"
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/OSSIA2iscore.hpp>

namespace Engine
{
namespace Network
{
MIDIDevice::MIDIDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  using namespace ossia;
  m_capas.canRefreshTree = true;
  m_capas.canSerialize = false;
  m_capas.hasCallbacks = false;

  reconnect();
}

bool MIDIDevice::reconnect()
{
  disconnect();
  m_dev.reset();

  MIDISpecificSettings set
      = settings().deviceSpecificSettings.value<MIDISpecificSettings>();
  try
  {
    auto proto = std::make_unique<ossia::net::midi::midi_protocol>();
    bool res = proto->setInfo(ossia::net::midi::midi_info(
        static_cast<ossia::net::midi::midi_info::Type>(set.io),
        set.endpoint.toStdString(),
        set.port));
    if (!res)
      return false;

    auto dev
        = std::make_unique<ossia::net::midi::midi_device>(std::move(proto));
    dev->setName(settings().name.toStdString());
    dev->updateNamespace();
    m_dev = std::move(dev);
  }
  catch (std::exception& e)
  {
    qDebug() << e.what();
  }

  return connected();
}

void MIDIDevice::disconnect()
{
  if (connected())
  {
    removeListening_impl(
        m_dev->getRootNode(), State::Address{m_settings.name, {}});
  }

  m_callbacks.clear();
  m_dev.reset();
}

Device::Node MIDIDevice::refresh()
{
  Device::Node device_node{settings(), nullptr};

  if (!connected())
  {
    return device_node;
  }
  else
  {
    const auto& children = m_dev->getRootNode().children();
    device_node.reserve(children.size());
    for (const auto& node : children)
    {
      device_node.push_back(
          Engine::ossia_to_iscore::ToDeviceExplorer(*node.get()));
    }
  }

  device_node.get<Device::DeviceSettings>().name = settings().name;
  return device_node;
}
}
}
