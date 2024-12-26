// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MCUProtocolFactory.hpp"

#include "MCUDevice.hpp"
#include "MCUProtocolSettingsWidget.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/MCU/MCUSpecificSettings.hpp>
#include <Protocols/MIDIUtils.hpp>
#include <Protocols/Settings/Model.hpp>

#include <ossia-qt/invoke.hpp>

#include <QObject>
#include <QTimer>
#include <QUrl>
namespace Protocols
{

Device::ProtocolFactory::Flags MCUProtocolFactory::flags() const noexcept
{
  return Device::ProtocolFactory::EditingReloadsEverything;
}

QString MCUProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("MIDI Controller");
}

QString MCUProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl MCUProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/midi-controller-device.html");
}

Device::DeviceEnumerators
MCUProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {};
}

Device::DeviceInterface* MCUProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new MCUDevice{settings, plugin.networkContext(), ctx};
}

const Device::DeviceSettings& MCUProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "MCU";
    MCUSpecificSettings specif;
    specif.api = getCurrentAPI();
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* MCUProtocolFactory::makeSettingsWidget()
{
  return new MCUSettingsWidget;
}

Device::AddressDialog* MCUProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* MCUProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings&, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget*)
{
  return nullptr;
}

QVariant MCUProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MCUSpecificSettings>(visitor);
}

void MCUProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MCUSpecificSettings>(data, visitor);
}

bool MCUProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  // FIXME check if we can open the same device multiple times ?
  auto specif = a.deviceSpecificSettings.value<MCUSpecificSettings>();
  if(specif.input_handle.empty())
    return false;
  if(specif.output_handle.empty())
    return false;
  // FIXME improve when we have multiple devices in one control surface
  return specif.input_handle[0] != libremidi::port_information{}
         && specif.output_handle[0] != libremidi::port_information{};
}
}
