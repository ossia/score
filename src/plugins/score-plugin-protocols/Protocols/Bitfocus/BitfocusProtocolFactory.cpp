// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusProtocolFactory.hpp"

#include "BitfocusDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Library/LibrarySettings.hpp>
#include <Protocols/Bitfocus/BitfocusEnumerator.hpp>
#include <Protocols/Bitfocus/BitfocusProtocolSettingsWidget.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/network/sockets/configuration.hpp>

#include <QObject>
#include <QUrl>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}
struct VisitorVariant;

namespace Protocols
{
QString BitfocusProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Bitfocus");
}

QString BitfocusProtocolFactory::category() const noexcept
{
  return StandardCategories::osc;
}

QUrl BitfocusProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/bitfocus-device.html");
}

Device::DeviceEnumerators
BitfocusProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  QString path = ctx.app.settings<Library::Settings::Model>().getPackagesPath()
                 + "/companion-modules/companion-bundled-modules";
  return {{"Devices", new BitfocusEnumerator{path, ctx}}};
}

Device::DeviceInterface* BitfocusProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new BitfocusDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& BitfocusProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Bitfocus";
    BitfocusSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* BitfocusProtocolFactory::makeSettingsWidget()
{
  return new BitfocusProtocolSettingsWidget;
}

QVariant BitfocusProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<BitfocusSpecificSettings>(visitor);
}

void BitfocusProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<BitfocusSpecificSettings>(data, visitor);
}

bool BitfocusProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}
}
