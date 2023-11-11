#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIODevice.hpp"
#include "SimpleIOProtocolFactory.hpp"
#include "SimpleIOProtocolSettingsWidget.hpp"
#include "SimpleIOSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QObject>

namespace Protocols
{

QString SimpleIOProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("SimpleIO");
}

QString SimpleIOProtocolFactory::category() const noexcept
{
  return StandardCategories::lights;
}

Device::DeviceInterface* SimpleIOProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new SimpleIODevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& SimpleIOProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "SimpleIO";
    SimpleIOSpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* SimpleIOProtocolFactory::makeSettingsWidget()
{
  return new SimpleIOProtocolSettingsWidget;
}

QVariant
SimpleIOProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<SimpleIOSpecificSettings>(visitor);
}

void SimpleIOProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<SimpleIOSpecificSettings>(data, visitor);
}

bool SimpleIOProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return false; //  TODO
}
}
#endif
