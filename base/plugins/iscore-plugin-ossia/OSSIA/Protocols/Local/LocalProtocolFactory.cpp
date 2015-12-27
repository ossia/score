#include <QObject>


#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include "LocalProtocolFactory.hpp"
#include "LocalProtocolSettingsWidget.hpp"
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

class DeviceInterface;
class ProtocolSettingsWidget;
struct VisitorVariant;

QString LocalProtocolFactory::prettyName() const
{
    return QObject::tr("Local");
}

const ProtocolFactoryKey& LocalProtocolFactory::key_impl() const
{
    static const ProtocolFactoryKey name{"Local"};
    return name;
}

DeviceInterface*LocalProtocolFactory::makeDevice(
        const iscore::DeviceSettings& settings,
        const iscore::DocumentContext& ctx)
{
    auto& app_plug = ctx.app.components.applicationPlugin<OSSIAApplicationPlugin>();
    return new LocalDevice{ctx, app_plug.localDevice(), settings};
}

const iscore::DeviceSettings& LocalProtocolFactory::defaultSettings() const
{
    static const iscore::DeviceSettings settings = [&] () {
        iscore::DeviceSettings s;
        s.protocol = key_impl(); // Todo check for un-set protocol.
        s.name = "i-score";
        LocalSpecificSettings specif;
        s.deviceSpecificSettings = QVariant::fromValue(specif);
        return s;
    }();
    return settings;
}

ProtocolSettingsWidget* LocalProtocolFactory::makeSettingsWidget()
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

bool LocalProtocolFactory::checkCompatibility(const iscore::DeviceSettings& a, const iscore::DeviceSettings& b) const
{
    return a.name != b.name;
}
