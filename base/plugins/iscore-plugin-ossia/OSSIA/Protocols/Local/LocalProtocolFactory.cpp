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


namespace Ossia
{
QString LocalProtocolFactory::prettyName() const
{
    return QObject::tr("Local");
}

Device::DeviceInterface* LocalProtocolFactory::makeDevice(
        const Device::DeviceSettings& settings,
        const iscore::DocumentContext& ctx)
{
    auto& app_plug = ctx.app.components.applicationPlugin<OSSIAApplicationPlugin>();
    return new LocalDevice{ctx, app_plug.localDevice(), settings};
}

const Device::DeviceSettings& LocalProtocolFactory::defaultSettings() const
{
    static const Device::DeviceSettings settings = [&] () {
        Device::DeviceSettings s;
        s.protocol = concreteFactoryKey(); // Todo check for un-set protocol.
        s.name = "i-score";
        LocalSpecificSettings specif;
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
    return makeProtocolSpecificSettings_T<LocalSpecificSettings>(visitor);
}

void LocalProtocolFactory::serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const
{
    serializeProtocolSpecificSettings_T<LocalSpecificSettings>(data, visitor);
}

bool LocalProtocolFactory::checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
    return a.name != b.name;
}
}
