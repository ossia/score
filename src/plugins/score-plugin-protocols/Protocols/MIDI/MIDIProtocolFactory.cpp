// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIProtocolFactory.hpp"

#include "MIDIDevice.hpp"
#include "MIDIProtocolSettingsWidget.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <Protocols/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia-qt/invoke.hpp>

#include <QObject>

#include <libremidi/libremidi.hpp>
namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}

struct VisitorVariant;

namespace Protocols
{
static Device::DeviceSettings
to_settings(libremidi::API api, const libremidi::input_port& p)
{
  Device::DeviceSettings set;
  MIDISpecificSettings specif;
  set.name = QString::fromStdString(p.display_name);
  set.protocol = MIDIInputProtocolFactory::static_concreteKey();
  specif.handle = p;

  specif.io = MIDISpecificSettings::IO::In;
  specif.api = api;

  set.deviceSpecificSettings = QVariant::fromValue(specif);

  return set;
}

static Device::DeviceSettings
to_settings(libremidi::API api, const libremidi::output_port& p)
{
  Device::DeviceSettings set;
  MIDISpecificSettings specif;
  set.name = QString::fromStdString(p.display_name);
  set.protocol = MIDIOutputProtocolFactory::static_concreteKey();
  specif.handle = p;

  specif.io = MIDISpecificSettings::IO::Out;
  specif.api = api;

  set.deviceSpecificSettings = QVariant::fromValue(specif);

  return set;
}

template <ossia::net::midi::midi_info::Type Type>
class MidiEnumerator : public Device::DeviceEnumerator
{
  libremidi::API m_api = [] {
    auto api
        = score::AppContext().settings<Protocols::Settings::Model>().getMidiApiAsEnum();
    if(api == libremidi::API::UNSPECIFIED)
      api = libremidi::midi1::default_api();
    return api;
  }();

  libremidi::observer_configuration make_callbacks(libremidi::observer_configuration& cb)
  {
    cb.notify_in_constructor = true;

    if constexpr(Type == ossia::net::midi::midi_info::Type::Input)
    {
      cb.input_added = [this](const libremidi::input_port& p) {
        ossia::qt::run_async(
            this, [this, s = to_settings(m_api, p)] { deviceAdded(s.name, s); });
      };
    }
    else
    {
      cb.output_added = [this](const libremidi::output_port& p) {
        ossia::qt::run_async(
            this, [this, s = to_settings(m_api, p)] { deviceAdded(s.name, s); });
      };
    }
    return cb;
  }

  libremidi::observer m_observer;

public:
  explicit MidiEnumerator(libremidi::observer_configuration& cb)
      : m_observer{make_callbacks(cb), libremidi::observer_configuration_for(m_api)}
  {
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
  }
};

class MidiKeyboardEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    Device::DeviceSettings set;
    MIDISpecificSettings specif;
    set.name = QString::fromStdString("Computer keyboard");
    set.protocol = MIDIInputProtocolFactory::static_concreteKey();
    specif.handle = libremidi::port_information{.display_name = "Computer keyboard"};

    specif.io = MIDISpecificSettings::IO::In;
    specif.api = libremidi::API::KEYBOARD;

    set.deviceSpecificSettings = QVariant::fromValue(specif);

    f("Computer keyboard", set);
  }
};

Device::ProtocolFactory::Flags MIDIInputProtocolFactory::flags() const noexcept
{
  return Device::ProtocolFactory::EditingReloadsEverything;
}

QString MIDIInputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("MIDI Input");
}

QString MIDIInputProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerators
MIDIInputProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  libremidi::observer_configuration obs_hw, obs_sw;
  obs_hw.track_hardware = true;
  obs_hw.track_virtual = false;
  obs_sw.track_hardware = false;
  obs_sw.track_virtual = true;
  return {
      {"Hardware inputs",
       new MidiEnumerator<ossia::net::midi::midi_info::Type::Input>(obs_hw)},
      {"Software inputs",
       new MidiEnumerator<ossia::net::midi::midi_info::Type::Input>(obs_sw)},
      {"Other", new MidiKeyboardEnumerator},
  };
}

Device::DeviceInterface* MIDIInputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new MIDIDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& MIDIInputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Midi";
    MIDISpecificSettings specif;
    specif.io = MIDISpecificSettings::IO::In;
    specif.virtualPort = false;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* MIDIInputProtocolFactory::makeSettingsWidget()
{
  return new MIDIInputSettingsWidget;
}

Device::AddressDialog* MIDIInputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* MIDIInputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings&, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget*)
{
  return nullptr;
}

QVariant MIDIInputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
}

void MIDIInputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
}

bool MIDIInputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  // FIXME check if we can open the same device multiple times ?
  auto specif = a.deviceSpecificSettings.value<MIDISpecificSettings>();
  return specif.handle != libremidi::port_information{} || specif.virtualPort;
}

QString MIDIOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("MIDI Output");
}

QString MIDIOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerators
MIDIOutputProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  libremidi::observer_configuration obs_hw, obs_sw;
  obs_hw.track_hardware = true;
  obs_hw.track_virtual = false;
  obs_sw.track_hardware = false;
  obs_sw.track_virtual = true;
  return {
      {"Hardware outputs",
       new MidiEnumerator<ossia::net::midi::midi_info::Type::Output>(obs_hw)},
      {"Software outputs",
       new MidiEnumerator<ossia::net::midi::midi_info::Type::Output>(obs_sw)},
  };
}

Device::DeviceInterface* MIDIOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new MIDIDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& MIDIOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Midi";
    MIDISpecificSettings specif;
    specif.io = MIDISpecificSettings::IO::Out;
    specif.virtualPort = false;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* MIDIOutputProtocolFactory::makeSettingsWidget()
{
  return new MIDIOutputSettingsWidget;
}

Device::AddressDialog* MIDIOutputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* MIDIOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings&, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget*)
{
  return nullptr;
}

QVariant MIDIOutputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
}

void MIDIOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
}

bool MIDIOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  // FIXME check if we can open the same device multiple times ?
  auto specif = a.deviceSpecificSettings.value<MIDISpecificSettings>();
  return specif.handle != libremidi::port_information{} || specif.virtualPort;
}
}
