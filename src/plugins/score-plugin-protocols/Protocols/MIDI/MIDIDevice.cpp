// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIDevice.hpp"

#include <State/MessageListSerialization.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/MIDI/MIDISpecificSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/protocols/midi/midi.hpp>

#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMimeData>

// clang-format off
#include <libremidi/libremidi.hpp>
#include <libremidi/backends.hpp>
#include <libremidi/configurations.hpp>
// clang-format on

#include <memory>

namespace Protocols
{
class MidiKeyboardEventFilter : public QObject
{
public:
  libremidi::kbd_input_configuration::scancode_callback press, release;
  MidiKeyboardEventFilter(
      libremidi::kbd_input_configuration::scancode_callback press,
      libremidi::kbd_input_configuration::scancode_callback release)
      : press{std::move(press)}
      , release{std::move(release)}
      , target{score::GUIAppContext().mainWindow}
  {
    // X11 quirk
    if(qApp->platformName() == "xcb")
      scanCodeOffset = -8;
  }

  bool eventFilter(QObject* object, QEvent* event)
  {
    if(object == target)
    {
      if(event->type() == QEvent::KeyPress)
      {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if(!keyEvent->isAutoRepeat())
          press(keyEvent->nativeScanCode() + scanCodeOffset);
      }
      else if(event->type() == QEvent::KeyRelease)
      {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if(!keyEvent->isAutoRepeat())
          release(keyEvent->nativeScanCode() + scanCodeOffset);
      }
    }
    return false;
  }
  QObject* target{};
  int scanCodeOffset{0};
};

MIDIDevice::MIDIDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
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

MIDIDevice::~MIDIDevice()
{
  delete m_kbdfilter;
}

bool MIDIDevice::reconnect()
{
  disconnect();

  MIDISpecificSettings set
      = settings().deviceSpecificSettings.value<MIDISpecificSettings>();

  m_capas.canSerialize = !set.createWholeTree;
  try
  {
    std::unique_ptr<ossia::net::midi::midi_protocol> proto;

    if(set.io == MIDISpecificSettings::IO::In)
    {
      libremidi::input_configuration conf;
      auto api_conf = libremidi::midi_in_configuration_for(set.api);
      libremidi::midi_any::for_input_configuration([&](auto& conf) {
        if constexpr(requires { conf.client_name; })
          conf.client_name = "ossia score";
      }, api_conf);

      switch(set.api)
      {
        case libremidi::API::ALSA_SEQ: {
          conf.timestamps = libremidi::timestamp_mode::AudioFrame;

          auto ptr = std::any_cast<libremidi::alsa_seq::input_configuration>(&api_conf);
          SCORE_ASSERT(ptr);
          ptr->client_name = "ossia score";
          break;
        }
        case libremidi::API::JACK_MIDI: {
          conf.timestamps = libremidi::timestamp_mode::AudioFrame;
          break;
        }
        case libremidi::API::PIPEWIRE: {
          conf.timestamps = libremidi::timestamp_mode::AudioFrame;
          break;
        }
        case libremidi::API::KEYBOARD: {
          auto ptr = std::any_cast<libremidi::kbd_input_configuration>(&api_conf);
          SCORE_ASSERT(ptr);
          ptr->set_input_scancode_callbacks = [this](auto keypress, auto keyrelease) {
            m_kbdfilter = new MidiKeyboardEventFilter{keypress, keyrelease};
            qApp->installEventFilter(m_kbdfilter);
          };
          break;
        }
        default:
          break;
      }
      // FIXME get the frame time in here in some way.
      // MIDIDevice needs to go in a plug-in after exec plugin, but is depended-on by dataflow
      // for access to the midi ports...
      // Note: which time do we use when no audio engine is running?
      // else
      // {
      //   input_conf.timestamps = libremidi::timestamp_mode::Custom;
      //   input_conf.get_timestamp = [this](int64_t) { return 0; };
      // }

      proto = std::make_unique<ossia::net::midi::midi_protocol>(
          m_ctx, set.handle.display_name, conf, api_conf);
    }
    else
    {
      libremidi::output_configuration conf;
      auto api_conf = libremidi::midi_out_configuration_for(set.api);
      libremidi::midi_any::for_output_configuration([&](auto& conf) {
        if constexpr(requires { conf.client_name; })
          conf.client_name = "ossia score";
      }, api_conf);
      proto = std::make_unique<ossia::net::midi::midi_protocol>(
          m_ctx, set.handle.display_name, conf, api_conf);
    }

    auto& p = *proto;

    auto dev = std::make_unique<ossia::net::midi::midi_device>(
        settings().name.toStdString(), std::move(proto));
    bool res = p.set_info(ossia::net::midi::midi_info{
        static_cast<ossia::net::midi::midi_info::Type>(set.io), set.handle,
        set.virtualPort});
    if(!res)
      return false;
    if(set.createWholeTree)
      dev->create_full_tree();
    m_dev = std::move(dev);
    deviceChanged(nullptr, m_dev.get());
  }
  catch(std::exception& e)
  {
    qDebug() << e.what();
  }

  return connected();
}

void MIDIDevice::disconnect()
{
  if(m_kbdfilter)
  {
    delete m_kbdfilter;
    m_kbdfilter = nullptr;
  }

  if(connected())
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

  if(!connected())
  {
    return device_node;
  }
  else
  {
    const auto& children = m_dev->get_root_node().children();
    device_node.reserve(children.size());
    for(const auto& node : children)
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
  if(!m_dev)
    return;
  auto& proto = static_cast<ossia::net::midi::midi_protocol&>(m_dev->get_protocol());
  auto& dev = *m_dev;
  if(b)
  {
    dev.on_node_created.connect<&DeviceInterface::nodeCreated>((DeviceInterface*)this);
    dev.on_node_removing.connect<&DeviceInterface::nodeRemoving>((DeviceInterface*)this);
    dev.on_node_renamed.connect<&DeviceInterface::nodeRenamed>((DeviceInterface*)this);
    dev.on_parameter_created.connect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.connect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }
  else
  {
    dev.on_node_created.disconnect<&DeviceInterface::nodeCreated>(
        (DeviceInterface*)this);
    dev.on_node_removing.disconnect<&DeviceInterface::nodeRemoving>(
        (DeviceInterface*)this);
    dev.on_node_renamed.disconnect<&DeviceInterface::nodeRenamed>(
        (DeviceInterface*)this);
    dev.on_parameter_created.disconnect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.disconnect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }

  proto.set_learning(b);
}
}
