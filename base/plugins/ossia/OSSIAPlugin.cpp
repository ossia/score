#include "OSSIAPlugin.hpp"
#include "OSSIAControl.hpp"

#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>


OSSIAPlugin::OSSIAPlugin() :
    QObject {}
{
}

iscore::PluginControlInterface* OSSIAPlugin::control()
{
    return new OSSIAControl{nullptr};
}



QVector<iscore::FactoryInterface*> OSSIAPlugin::factories(const QString& factoryName)
{
    if(factoryName == "Protocol")
    {
        return {new MIDIProtocolFactory,
                new MinuitProtocolFactory,
                new OSCProtocolFactory};
    }

    return {};
}
