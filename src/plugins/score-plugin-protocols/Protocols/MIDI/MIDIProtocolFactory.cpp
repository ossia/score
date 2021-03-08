// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIProtocolFactory.hpp"

#include "MIDIDevice.hpp"
#include "MIDIProtocolSettingsWidget.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>

#include <QObject>
#if defined(__EMSCRIPTEN__)
#define LIBREMIDI_HEADER_ONLY 1
#define LIBREMIDI_EMSCRIPTEN 1
#include <libremidi/libremidi.hpp>
#endif
namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}

struct VisitorVariant;

namespace Protocols
{
template<ossia::net::midi::midi_info::Type Type>
class MidiEnumerator : public Device::DeviceEnumerator
{

#if defined(__EMSCRIPTEN__)
  libremidi::observer::callbacks make_callbacks()
  {
    libremidi::observer::callbacks cb;
    if constexpr (Type == ossia::net::midi::midi_info::Type::Input)
    {
      cb.input_added = [this] (int port, const std::string& device) {
        Device::DeviceSettings set;
        MIDISpecificSettings specif;
        set.name = QString::fromStdString(device);
        specif.endpoint = QString::fromStdString(device);

        set.protocol = MIDIInputProtocolFactory::static_concreteKey();
        specif.io = MIDISpecificSettings::IO::In;

        specif.port = port;
        set.deviceSpecificSettings = QVariant::fromValue(specif);

        deviceAdded(set);
      };
    }
    else
    {
      cb.output_added = [this] (int port, const std::string& device) {
        Device::DeviceSettings set;
        MIDISpecificSettings specif;
        set.name = QString::fromStdString(device);
        specif.endpoint = QString::fromStdString(device);

        set.protocol = MIDIOutputProtocolFactory::static_concreteKey();
        specif.io = MIDISpecificSettings::IO::Out;
        specif.port = port;
        set.deviceSpecificSettings = QVariant::fromValue(specif);

        deviceAdded(set);
      };
    }
    return cb;
  }

  libremidi::observer m_observer;
public:
  MidiEnumerator()
    : m_observer{libremidi::API::EMSCRIPTEN_WEBMIDI, make_callbacks()}
  {

  }
#endif
  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
  {
    try
    {
      auto prot = std::make_unique<ossia::net::midi::midi_protocol>();
      auto vec = prot->scan();

      for (auto& elt : vec)
      {
        if (elt.type == Type)
        {
          Device::DeviceSettings set;
          MIDISpecificSettings specif;
          set.name = QString::fromStdString(elt.device);
          specif.endpoint = QString::fromStdString(elt.device);

          if constexpr (Type == ossia::net::midi::midi_info::Type::Input)
          {
            set.protocol = MIDIInputProtocolFactory::static_concreteKey();
            specif.io = MIDISpecificSettings::IO::In;
          }
          else
          {
            set.protocol = MIDIOutputProtocolFactory::static_concreteKey();
            specif.io = MIDISpecificSettings::IO::Out;
          }

          specif.port = elt.port;
          set.deviceSpecificSettings = QVariant::fromValue(specif);

          f(set);
        }
      }
    }
    catch (std::exception& e)
    {
      qDebug() << e.what();
    }
  }
};

QString MIDIInputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("MIDI Input");
}

QString MIDIInputProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerator* MIDIInputProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new MidiEnumerator<ossia::net::midi::midi_info::Type::Input>;
}

Device::DeviceInterface* MIDIInputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new MIDIDevice{settings};
}

const Device::DeviceSettings& MIDIInputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Midi";
    MIDISpecificSettings specif;
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
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* MIDIInputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings&,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget*)
{
  return nullptr;
}

QVariant MIDIInputProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
}

void MIDIInputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
}

bool MIDIInputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

QString MIDIOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("MIDI Output");
}

QString MIDIOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerator* MIDIOutputProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new MidiEnumerator<ossia::net::midi::midi_info::Type::Output>;
}

Device::DeviceInterface* MIDIOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new MIDIDevice{settings};
}

const Device::DeviceSettings& MIDIOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Midi";
    MIDISpecificSettings specif;
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
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* MIDIOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings&,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget*)
{
  return nullptr;
}

QVariant MIDIOutputProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
}

void MIDIOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
}

bool MIDIOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}
}
