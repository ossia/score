#include <QObject>


#include <Device/Protocol/DeviceSettings.hpp>
#include "MinuitDevice.hpp"
#include "MinuitProtocolFactory.hpp"
#include "MinuitProtocolSettingsWidget.hpp"
#include "Protocols/Minuit/MinuitSpecificSettings.hpp"

class DeviceInterface;
class ProtocolSettingsWidget;
struct VisitorVariant;

QString MinuitProtocolFactory::prettyName() const
{
    return QObject::tr("Minuit");
}

const ProtocolFactoryKey&MinuitProtocolFactory::key_impl() const
{
    static const ProtocolFactoryKey name{"Minuit"};
    return name;
}

DeviceInterface*MinuitProtocolFactory::makeDevice(const iscore::DeviceSettings& settings)
{
    return new MinuitDevice{settings};
}

ProtocolSettingsWidget*MinuitProtocolFactory::makeSettingsWidget()
{
    return new MinuitProtocolSettingsWidget;
}

QVariant MinuitProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
    return makeProtocolSpecificSettings_T<MinuitSpecificSettings>(visitor);
}

void MinuitProtocolFactory::serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const
{
    serializeProtocolSpecificSettings_T<MinuitSpecificSettings>(data, visitor);
}

bool MinuitProtocolFactory::checkCompatibility(const iscore::DeviceSettings& a, const iscore::DeviceSettings& b) const
{
    // TODO we should check also for other devices.  Devices should have a "open ports" method that
    // returns the ports they use.
    auto a_p = a.deviceSpecificSettings.value<MinuitSpecificSettings>();
    auto b_p = b.deviceSpecificSettings.value<MinuitSpecificSettings>();
    return a.name != b.name && a_p.inputPort != b_p.inputPort && a_p.outputPort != b_p.outputPort;
}
