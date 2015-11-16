#include "iscore_plugin_ossia.hpp"
#include "OSSIAControl.hpp"

#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>

#include "DocumentPlugin/ContextMenu/PlayContextMenuFactory.hpp"

iscore_plugin_ossia::iscore_plugin_ossia() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_ossia::make_control(
        iscore::Application& app)
{
    return new OSSIAControl{app};
}



std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_ossia::factories(
        const iscore::ApplicationContext&,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProtocolFactory::staticFactoryKey())
    {
        return {//new MIDIProtocolFactory,
                new MinuitProtocolFactory,
                new OSCProtocolFactory};
    }

    if(factoryName == ScenarioActionsFactory::staticFactoryKey())
    {
        return {new PlayContextMenuFactory};
    }

    return {};
}


QStringList iscore_plugin_ossia::required() const
{
    return {"Scenario"};
}
