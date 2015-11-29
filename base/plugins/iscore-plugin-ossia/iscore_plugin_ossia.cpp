#include <Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>
#include <QString>

#include "Device/Protocol/ProtocolFactoryInterface.hpp"
#include "DocumentPlugin/ContextMenu/PlayContextMenuFactory.hpp"
#include "OSSIAApplicationPlugin.hpp"
#include "Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp"
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_ossia.hpp"

namespace iscore {
class Application;
}  // namespace iscore

iscore_plugin_ossia::iscore_plugin_ossia() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_plugin_ossia::make_applicationPlugin(
        iscore::Application& app)
{
    return new OSSIAApplicationPlugin{app};
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
