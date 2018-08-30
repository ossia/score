// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIDevice.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/midi/midi.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <score/serialization/MimeVisitor.hpp>
#include <State/MessageListSerialization.hpp>
#include <QMimeData>
#include <QString>
#include <memory>

namespace Engine
{
namespace Network
{
MIDIDevice::MIDIDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  using namespace ossia;
  m_capas.canRefreshTree = true;
  m_capas.canSerialize = false;
  m_capas.hasCallbacks = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
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
    bool res = proto->set_info(ossia::net::midi::midi_info(
        static_cast<ossia::net::midi::midi_info::Type>(set.io),
        set.endpoint.toStdString(), set.port));
    if (!res)
      return false;

    auto dev
        = std::make_unique<ossia::net::midi::midi_device>(std::move(proto));
    dev->set_name(settings().name.toStdString());
    dev->update_namespace();
    m_dev = std::move(dev);
    deviceChanged(nullptr, m_dev.get());
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
        m_dev->get_root_node(), State::Address{m_settings.name, {}});
  }

  m_callbacks.clear();
  m_dev.reset();
}

QMimeData* MIDIDevice::mimeData() const
{
  auto mimeData = new QMimeData;

  State::Message mess;
  mess.address.address.device = m_settings.name;

  Mime<State::MessageList>::Serializer s{*mimeData};
  s.serialize({mess});
  return mimeData;
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
    const auto& children = m_dev->get_root_node().children();
    device_node.reserve(children.size());
    for (const auto& node : children)
    {
      device_node.push_back(
          Device::ToDeviceExplorer(*node.get()));
    }
  }

  device_node.get<Device::DeviceSettings>().name = settings().name;
  return device_node;
}
}
}
