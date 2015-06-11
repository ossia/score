#include "OSSIAPlugin.hpp"
#include "OSSIAControl.hpp"

#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>


iscore_plugin_ossia::iscore_plugin_ossia() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_ossia::make_control(iscore::Presenter* pres)
{
    return new OSSIAControl{pres};
}



QVector<iscore::FactoryInterface*> iscore_plugin_ossia::factories(const QString& factoryName)
{
    if(factoryName == "Protocol")
    {
        return {//new MIDIProtocolFactory,
                new MinuitProtocolFactory,
                new OSCProtocolFactory};
    }

    return {};
}
