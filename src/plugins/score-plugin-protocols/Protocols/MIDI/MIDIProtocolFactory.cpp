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
QString MIDIProtocolFactory::prettyName() const
{
  return QObject::tr("MIDI");
}

Device::DeviceInterface* MIDIProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new MIDIDevice{settings};
}

const Device::DeviceSettings& MIDIProtocolFactory::defaultSettings() const
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

Device::ProtocolSettingsWidget* MIDIProtocolFactory::makeSettingsWidget()
{
  return new MIDIProtocolSettingsWidget;
}

Device::AddressDialog* MIDIProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* MIDIProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings&,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget*)
{
  return nullptr;
}

QVariant MIDIProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
}

void MIDIProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
}

bool MIDIProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}
}
