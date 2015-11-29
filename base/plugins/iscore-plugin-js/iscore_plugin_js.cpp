#include <JS/Inspector/JSInspectorFactory.hpp>
#include <JS/JSProcessFactory.hpp>
#include <unordered_map>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "JS/Commands/JSCommandFactory.hpp"
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_js.hpp"
#include <iscore_plugin_js_commands_files.hpp>

iscore_plugin_js::iscore_plugin_js() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_js::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new JSProcessFactory};
    }

    if(factoryName == InspectorWidgetFactory::staticFactoryKey())
    {
        return {new JSInspectorFactory};
    }

    return {};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_js::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{JSCommandFactoryName(), CommandGeneratorMap{}};

    using Types = iscore::commands::TypeList<
#include <iscore_plugin_js_commands.hpp>
      >;
    iscore::commands::ForEach<Types>(iscore::commands::FactoryInserter{cmds.second});


    return cmds;
}
