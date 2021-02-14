// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <State/MessageListSerialization.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/document/DocumentContext.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/protocols/midi/midi.hpp>

#include <QDebug>
#include <QMimeData>

#include <memory>

namespace Protocols
{
MIDIDevice::MIDIDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
  : OwningDeviceInterface{settings}
  , m_ctx{ctx}
{
  using namespace ossia;

  const auto set = settings.deviceSpecificSettings.value<MIDISpecificSettings>();
  m_capas.canRefreshTree = true;
  m_capas.canSerialize = !set.createWholeTree;
  m_capas.hasCallbacks = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canLearn = true;
}

bool MIDIDevice::reconnect()
{
  disconnect();
  auto old = m_dev.get();
  deviceChanged(old, nullptr);
  m_dev.reset();

  MIDISpecificSettings set = settings().deviceSpecificSettings.value<MIDISpecificSettings>();

  m_capas.canSerialize = !set.createWholeTree;
  try
  {
    auto& ctx = m_ctx.plugin<Explorer::DeviceDocumentPlugin>().asioContext;

    auto proto = std::make_unique<ossia::net::midi::midi_protocol>(ctx);
    bool res = proto->set_info(ossia::net::midi::midi_info(
        static_cast<ossia::net::midi::midi_info::Type>(set.io),
        set.endpoint.toStdString(),
        set.port));
    if (!res)
      return false;

    auto dev = std::make_unique<ossia::net::midi::midi_device>(std::move(proto));
    dev->set_name(settings().name.toStdString());
    if (set.createWholeTree)
      dev->create_full_tree();
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
    removeListening_impl(m_dev->get_root_node(), State::Address{m_settings.name, {}});
  }

  m_callbacks.clear();
  auto old = m_dev.get();
  m_dev.reset();
  deviceChanged(old, nullptr);
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
      device_node.push_back(Device::ToDeviceExplorer(*node.get()));
    }
  }

  device_node.get<Device::DeviceSettings>().name = settings().name;
  return device_node;
}

bool MIDIDevice::isLearning() const
{
  auto& proto = static_cast<ossia::net::midi::midi_protocol&>(m_dev->get_protocol());
  return proto.learning();
}

void MIDIDevice::setLearning(bool b)
{
  if (!m_dev)
    return;
  auto& proto = static_cast<ossia::net::midi::midi_protocol&>(m_dev->get_protocol());
  auto& dev = *m_dev;
  if (b)
  {
    dev.on_node_created.connect<&DeviceInterface::nodeCreated>((DeviceInterface*)this);
    dev.on_node_removing.connect<&DeviceInterface::nodeRemoving>((DeviceInterface*)this);
    dev.on_node_renamed.connect<&DeviceInterface::nodeRenamed>((DeviceInterface*)this);
    dev.on_parameter_created.connect<&DeviceInterface::addressCreated>((DeviceInterface*)this);
    dev.on_attribute_modified.connect<&DeviceInterface::addressUpdated>((DeviceInterface*)this);
  }
  else
  {
    dev.on_node_created.disconnect<&DeviceInterface::nodeCreated>((DeviceInterface*)this);
    dev.on_node_removing.disconnect<&DeviceInterface::nodeRemoving>((DeviceInterface*)this);
    dev.on_node_renamed.disconnect<&DeviceInterface::nodeRenamed>((DeviceInterface*)this);
    dev.on_parameter_created.disconnect<&DeviceInterface::addressCreated>((DeviceInterface*)this);
    dev.on_attribute_modified.disconnect<&DeviceInterface::addressUpdated>((DeviceInterface*)this);
  }

  proto.set_learning(b);
}
}
