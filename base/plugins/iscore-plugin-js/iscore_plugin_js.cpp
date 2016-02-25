#include <JS/Inspector/JSInspectorFactory.hpp>
#include <JS/Executor/Component.hpp>
#include <JS/Executor/StateComponent.hpp>
#include <JS/JSProcessFactory.hpp>
#include <JS/StateProcess.hpp>
#include <unordered_map>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "JS/Commands/JSCommandFactory.hpp"
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_js.hpp"
#include <iscore_plugin_js_commands_files.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

iscore_plugin_js::iscore_plugin_js() :
    QObject {}
{
}

iscore_plugin_js::~iscore_plugin_js()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_js::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<Process::ProcessFactory,
             JS::ProcessFactory>,
        FW<Process::StateProcessFactory,
             JS::StateProcessFactory>,
        FW<Process::InspectorWidgetDelegateFactory,
             JS::InspectorFactory>,
        FW<Process::StateProcessInspectorWidgetDelegateFactory,
             JS::StateInspectorFactory>,
        FW<RecreateOnPlay::ProcessComponentFactory,
             JS::Executor::ProcessComponentFactory>,
        FW<RecreateOnPlay::StateProcessComponentFactory,
             JS::Executor::StateProcessComponentFactory>
    >>(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_js::make_commands()
{
    using namespace JS;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{
        JS::CommandFactoryName(),
                CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_js_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});


    return cmds;
}

iscore::Version iscore_plugin_js::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_js::key() const
{
    return "0eb1db4b-a532-4961-ba1c-d9edbf08ef07";
}
