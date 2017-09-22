#include "score_addon_skeleton.hpp"
#include <score_addon_skeleton_commands_files.hpp>

#include <Skeleton/Process.hpp>
#include <Skeleton/Executor.hpp>
#include <Skeleton/Inspector.hpp>
#include <Skeleton/LocalTree.hpp>
#include <Skeleton/Layer.hpp>
#include <Skeleton/CommandFactory.hpp>

#include <score/plugins/customfactory/FactorySetup.hpp>

score_addon_skeleton::score_addon_skeleton()
{

}

score_addon_skeleton::~score_addon_skeleton()
{

}

std::vector<std::unique_ptr<score::InterfaceBase> >
score_addon_skeleton::factories(
        const score::ApplicationContext& ctx,
        const score::InterfaceKey& key) const
{
    return instantiate_factories<
            score::ApplicationContext,
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
score_addon_skeleton::make_commands()
{
    using namespace Skeleton;
    std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
        CommandFactoryName(),
        CommandGeneratorMap{}};

    using Types = TypeList<
#include <score_addon_skeleton_commands.hpp>
      >;

    for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

    return cmds;
}
