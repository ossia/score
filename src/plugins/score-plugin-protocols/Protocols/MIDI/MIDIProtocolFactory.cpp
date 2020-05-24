// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIProtocolFactory.hpp"

#include "MIDIDevice.hpp"
#include "MIDIProtocolSettingsWidget.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>

#include <QObject>

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
public:
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

          if constexpr (Type == ossia::net::midi::midi_info::Type::RemoteInput)
          {
            specif.io = MIDISpecificSettings::IO::In;
            set.protocol = MIDIInputProtocolFactory::static_concreteKey();
            specif.createWholeTree = true;
          }
          else
          {
            specif.io = MIDISpecificSettings::IO::Out;
            set.protocol = MIDIInputProtocolFactory::static_concreteKey();
          }

          specif.port = elt.port;
          set.deviceSpecificSettings = QVariant::fromValue(set);

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
  return new MidiEnumerator<ossia::net::midi::midi_info::Type::RemoteInput>;
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
  return new MidiEnumerator<ossia::net::midi::midi_info::Type::RemoteOutput>;
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
