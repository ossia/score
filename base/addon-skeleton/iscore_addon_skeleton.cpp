#include "iscore_addon_skeleton.hpp"
#include <iscore_addon_skeleton_commands_files.hpp>

#include <Skeleton/Process.hpp>
#include <Skeleton/Executor.hpp>
#include <Skeleton/Inspector.hpp>
#include <Skeleton/LocalTree.hpp>
#include <Skeleton/Layer.hpp>
#include <Skeleton/CommandFactory.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_addon_skeleton::iscore_addon_skeleton()
{

}

iscore_addon_skeleton::~iscore_addon_skeleton()
{

}

std::vector<std::unique_ptr<iscore::InterfaceBase> >
iscore_addon_skeleton::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::InterfaceKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
        FW<Process::ProcessModelFactory,
           Skeleton::ProcessFactory>,
        FW<Process::LayerFactory,
           Skeleton::LayerFactory>,
        FW<Process::InspectorWidgetDelegateFactory,
           Skeleton::InspectorFactory>,
        FW<Engine::Execution::ProcessComponentFactory,
           Skeleton::ProcessExecutorComponentFactory>,
        FW<Engine::LocalTree::ProcessComponentFactory,
           Skeleton::LocalTreeProcessComponentFactory>
    >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_addon_skeleton::make_commands()
{
    using namespace Skeleton;
    std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
        CommandFactoryName(),
        CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_addon_skeleton_commands.hpp>
      >;

    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
