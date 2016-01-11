#include <OSSIA/Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <OSSIA/Protocols/OSC/OSCProtocolFactory.hpp>
#include <OSSIA/Protocols/Local/LocalProtocolFactory.hpp>
#include <QString>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <OSSIA/Executor/ContextMenu/PlayContextMenuFactory.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

#include <OSSIA/LocalTree/Scenario/ScenarioComponentFactory.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ScenarioElement.hpp>
#include <OSSIA/Executor/LoopElement.hpp>
#include <OSSIA/Executor/AutomationElement.hpp>
#include <OSSIA/Executor/MappingElement.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_ossia.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
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

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_ossia::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Ossia::LocalTree::ProcessComponentFactoryList,
            RecreateOnPlay::ProcessComponentFactoryList
            >();
}



std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_ossia::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{

    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<ProtocolFactory,
                 LocalProtocolFactory,
                 OSCProtocolFactory,
                 MinuitProtocolFactory>,
            FW<ScenarioActionsFactory,
                 PlayContextMenuFactory>,
            FW<RecreateOnPlay::ProcessComponentFactory,
                 RecreateOnPlay::ScenarioComponentFactory,
                 RecreateOnPlay::LoopComponentFactory,
                 RecreateOnPlay::MappingComponentFactory
            >,
            FW<Ossia::LocalTree::ProcessComponentFactory,
                 Ossia::LocalTree::ScenarioComponentFactory>
            >
            >(ctx, key);
}


QStringList iscore_plugin_ossia::required() const
{
    return {"Scenario"};
}
