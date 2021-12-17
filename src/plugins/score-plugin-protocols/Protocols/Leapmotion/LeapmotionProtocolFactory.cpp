
#include "LeapmotionProtocolFactory.hpp"

#include "LeapmotionDevice.hpp"
#include "LeapmotionProtocolSettingsWidget.hpp"
#include "LeapmotionSpecificSettings.hpp"

#include <QObject>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Protocols
{

QString LeapmotionProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Leapmotion");
}

QString LeapmotionProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerator*
LeapmotionProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* LeapmotionProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LeapmotionDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings&
LeapmotionProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Leapmotion";
    LeapmotionSpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* LeapmotionProtocolFactory::makeSettingsWidget()
{
  return new LeapmotionProtocolSettingsWidget;
}

QVariant LeapmotionProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LeapmotionSpecificSettings>(visitor);
}

void LeapmotionProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LeapmotionSpecificSettings>(data, visitor);
}

bool LeapmotionProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return false;
}

}
