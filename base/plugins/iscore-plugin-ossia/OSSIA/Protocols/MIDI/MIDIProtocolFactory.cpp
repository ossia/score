#include <QObject>


#include <Device/Protocol/DeviceSettings.hpp>
#include "MIDIDevice.hpp"
#include "MIDIProtocolFactory.hpp"
#include "MIDIProtocolSettingsWidget.hpp"
#include <OSSIA/Protocols/MIDI/MIDISpecificSettings.hpp>

class DeviceInterface;
class ProtocolSettingsWidget;
struct VisitorVariant;

QString MIDIProtocolFactory::prettyName() const
{
    return QObject::tr("MIDI");
}

const ProtocolFactoryKey&MIDIProtocolFactory::key_impl() const
{
    static const ProtocolFactoryKey name{"MIDI"};
    return name;
}

DeviceInterface*MIDIProtocolFactory::makeDevice(
        const iscore::DeviceSettings& settings,
        const iscore::DocumentContext& ctx)
{
    return new MIDIDevice{settings};
}

const iscore::DeviceSettings& MIDIProtocolFactory::defaultSettings() const
{
    static const iscore::DeviceSettings settings = [&] () {
        iscore::DeviceSettings s;
        s.protocol = key_impl();
        s.name = "Midi";
        MIDISpecificSettings specif;
        s.deviceSpecificSettings = QVariant::fromValue(specif);
        return s;
    }();
    return settings;
}

ProtocolSettingsWidget*MIDIProtocolFactory::makeSettingsWidget()
{
    return new MIDIProtocolSettingsWidget;
}

QVariant MIDIProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
    return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
}

void MIDIProtocolFactory::serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const
{
    serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
}

bool MIDIProtocolFactory::checkCompatibility(const iscore::DeviceSettings& a, const iscore::DeviceSettings& b) const
{
    return a.name != b.name;
}
