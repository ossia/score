// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>

#include "LocalDevice.hpp"
#include "LocalProtocolFactory.hpp"
#include "LocalProtocolSettingsWidget.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Engine/Protocols/Local/LocalSpecificSettings.hpp>
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
QString LocalProtocolFactory::prettyName() const
{
  return QObject::tr("Local");
}

Device::DeviceInterface* LocalProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const iscore::DocumentContext& ctx)
{
  auto doc = ctx.findPlugin<LocalTree::DocumentPlugin>();
  if (doc)
  {
    doc->localDevice().updateSettings(settings);
    return &doc->localDevice();
  }
  else
    return nullptr;
}

const Device::DeviceSettings& LocalProtocolFactory::static_defaultSettings()
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol
        = static_concreteKey(); // Todo check for un-set protocol.
    s.name = "i-score";
    Network::LocalSpecificSettings specif;
    specif.host = "127.0.0.1";
    specif.remoteName = "i-score-remote";
    specif.localPort = 6666;
    specif.remotePort = 9999;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();

  return settings;
}

const Device::DeviceSettings& LocalProtocolFactory::defaultSettings() const
{
  return static_defaultSettings();
}

Device::ProtocolSettingsWidget* LocalProtocolFactory::makeSettingsWidget()
{
  return new LocalProtocolSettingsWidget;
}

QVariant LocalProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<Network::LocalSpecificSettings>(
      visitor);
}

void LocalProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<Network::LocalSpecificSettings>(
      data, visitor);
}

bool LocalProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}
}
}
