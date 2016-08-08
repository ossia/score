#include <QObject>


#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include "LocalProtocolFactory.hpp"
#include "LocalProtocolSettingsWidget.hpp"
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

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
        const Device::DeviceSettings& settings,
        const iscore::DocumentContext& ctx)
{
    return new Network::LocalDevice{ctx, settings};
}

const Device::DeviceSettings& LocalProtocolFactory::defaultSettings() const
{
    static const Device::DeviceSettings settings = [&] () {
        Device::DeviceSettings s;
        s.protocol = concreteFactoryKey(); // Todo check for un-set protocol.
        s.name = "i-score";
        Network::LocalSpecificSettings specif;
        s.deviceSpecificSettings = QVariant::fromValue(specif);
        return s;
    }();
    return settings;
}

Device::ProtocolSettingsWidget* LocalProtocolFactory::makeSettingsWidget()
{
    return new LocalProtocolSettingsWidget;
}

QVariant LocalProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
    return makeProtocolSpecificSettings_T<Network::LocalSpecificSettings>(visitor);
}

void LocalProtocolFactory::serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const
{
    serializeProtocolSpecificSettings_T<Network::LocalSpecificSettings>(data, visitor);
}

bool LocalProtocolFactory::checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
    return a.name != b.name;
}
}
}
