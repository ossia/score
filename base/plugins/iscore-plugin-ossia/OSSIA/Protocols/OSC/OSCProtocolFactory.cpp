#include <QObject>


#include <Device/Protocol/DeviceSettings.hpp>
#include "OSCDevice.hpp"
#include "OSCProtocolFactory.hpp"
#include "OSCProtocolSettingsWidget.hpp"
#include <OSSIA/Protocols/OSC/OSCSpecificSettings.hpp>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}
struct VisitorVariant;

namespace Ossia
{
QString OSCProtocolFactory::prettyName() const
{
    return QObject::tr("OSC");
}

const Device::ProtocolFactoryKey& OSCProtocolFactory::concreteFactoryKey() const
{
    static const Device::ProtocolFactoryKey name{"9a42de4b-f6eb-4bca-9564-01b975f601b9"};
    return name;
}

Device::DeviceInterface* OSCProtocolFactory::makeDevice(
        const Device::DeviceSettings& settings,
        const iscore::DocumentContext& ctx)
{
    return new OSCDevice{settings};
}

const Device::DeviceSettings& OSCProtocolFactory::defaultSettings() const
{
    static const Device::DeviceSettings settings = [&] () {
        Device::DeviceSettings s;
        s.protocol = concreteFactoryKey();
        s.name = "OSC";
        OSCSpecificSettings specif;
        s.deviceSpecificSettings = QVariant::fromValue(specif);
        return s;
    }();
    return settings;
}

Device::ProtocolSettingsWidget* OSCProtocolFactory::makeSettingsWidget()
{
    return new OSCProtocolSettingsWidget;
}

QVariant OSCProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
    return makeProtocolSpecificSettings_T<OSCSpecificSettings>(visitor);
}

void OSCProtocolFactory::serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const
{
    serializeProtocolSpecificSettings_T<OSCSpecificSettings>(data, visitor);
}

bool OSCProtocolFactory::checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
    auto a_p = a.deviceSpecificSettings.value<OSCSpecificSettings>();
    auto b_p = b.deviceSpecificSettings.value<OSCSpecificSettings>();
    return a.name != b.name && a_p.inputPort != b_p.inputPort && a_p.outputPort != b_p.outputPort;
}
}
