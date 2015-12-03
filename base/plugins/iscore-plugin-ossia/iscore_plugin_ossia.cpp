#include <OSSIA/Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <OSSIA/Protocols/OSC/OSCProtocolFactory.hpp>
#include <QString>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <OSSIA/DocumentPlugin/ContextMenu/PlayContextMenuFactory.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_ossia.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace iscore {

}  // namespace iscore

iscore_plugin_ossia::iscore_plugin_ossia() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_plugin_ossia::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    return new OSSIAApplicationPlugin{app};
}



std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_ossia::factories(
        const iscore::ApplicationContext&,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProtocolFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                MinuitProtocolFactory,
                OSCProtocolFactory>();
    }

    if(factoryName == ScenarioActionsFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                PlayContextMenuFactory>();
    }

    return {};
}


QStringList iscore_plugin_ossia::required() const
{
    return {"Scenario"};
}
