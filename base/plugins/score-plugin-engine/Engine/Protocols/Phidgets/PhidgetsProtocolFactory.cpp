// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>

#include "PhidgetsDevice.hpp"
#include "PhidgetsProtocolFactory.hpp"
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Phidgets/PhidgetsProtocolSettingsWidget.hpp>
#include <Engine/Protocols/Phidgets/PhidgetsSpecificSettings.hpp>
#include <ossia/network/base/device.hpp>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}

struct VisitorVariant;

namespace Engine
{
namespace Network
{
QString PhidgetProtocolFactory::prettyName() const
{
  return QObject::tr("Phidget");
}

Device::DeviceInterface* PhidgetProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
{
  return new PhidgetDevice{settings};
}

const Device::DeviceSettings& PhidgetProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Phidget";
    PhidgetSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* PhidgetProtocolFactory::makeSettingsWidget()
{
  return new PhidgetProtocolSettingsWidget;
}

QVariant PhidgetProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<PhidgetSpecificSettings>(visitor);
}

void PhidgetProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<PhidgetSpecificSettings>(data, visitor);
}

bool PhidgetProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}

Device::AddAddressDialog*PhidgetProtocolFactory::makeAddAddressDialog(const Device::DeviceSettings& dev, const score::DocumentContext& ctx, QWidget*)
{
  return nullptr;
}
}
}
