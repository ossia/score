#include "MIDIProtocolFactory.hpp"
#include "MIDIDevice.hpp"
#include "MIDIProtocolSettingsWidget.hpp"
QString MIDIProtocolFactory::prettyName() const
{
    return QObject::tr("MIDI");
}

const ProtocolFactoryKey&MIDIProtocolFactory::key_impl() const
{
    static const ProtocolFactoryKey name{"MIDI"};
    return name;
}

DeviceInterface*MIDIProtocolFactory::makeDevice(const iscore::DeviceSettings& settings)
{
    return new MIDIDevice{settings};
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
