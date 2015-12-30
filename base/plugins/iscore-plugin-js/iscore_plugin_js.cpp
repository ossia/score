#include <JS/Inspector/JSInspectorFactory.hpp>
#include <JS/JSProcessFactory.hpp>
#include <unordered_map>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "JS/Commands/JSCommandFactory.hpp"
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_js.hpp"
#include <iscore_plugin_js_commands_files.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_js::iscore_plugin_js() :
    QObject {}
{
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_js::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<Process::ProcessFactory,
             JS::ProcessFactory>,
        FW<ProcessInspectorWidgetDelegateFactory,
             JS::InspectorFactory>
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
