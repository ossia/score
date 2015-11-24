#include "iscore_plugin_loop.hpp"

#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopProcessFactory.hpp>

#include <Loop/Inspector/LoopInspectorFactory.hpp>

#include <Loop/Commands/LoopCommandFactory.hpp>

#include <iscore_plugin_loop_commands_files.hpp>

iscore_plugin_loop::iscore_plugin_loop() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_loop::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{
    if(key == ProcessFactory::staticFactoryKey())
    {
        return {new LoopProcessFactory};
    }

    if(key == InspectorWidgetFactory::staticFactoryKey())
    {
        return {new LoopInspectorFactory};
    }

    if(key == ConstraintInspectorDelegateFactory::staticFactoryKey())
    {
        return { new LoopConstraintInspectorDelegateFactory };
    }

    return {};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_loop::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{LoopCommandFactoryName(), CommandGeneratorMap{}};

    using Types = iscore::commands::TypeList<
  #include <iscore_plugin_loop_commands.hpp>
      >;
    iscore::commands::ForEach<Types>(iscore::commands::FactoryInserter{cmds.second});


    return cmds;
}
