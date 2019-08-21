#include "score_addon_nodal.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <Nodal/CommandFactory.hpp>
#include <Nodal/Executor.hpp>
#include <Nodal/Inspector.hpp>
#include <Nodal/Layer.hpp>
#include <Nodal/LocalTree.hpp>
#include <Nodal/Process.hpp>
#include <score_addon_nodal_commands_files.hpp>

score_addon_nodal::score_addon_nodal()
{
}

score_addon_nodal::~score_addon_nodal()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_addon_nodal::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Nodal::ProcessFactory>,
      FW<Process::LayerFactory, Nodal::LayerFactory>,
      FW<Process::InspectorWidgetDelegateFactory, Nodal::InspectorFactory>,
      FW<Execution::ProcessComponentFactory,
         Nodal::ProcessExecutorComponentFactory>,
      FW<score::ObjectRemover, Nodal::NodeRemover>
      //, FW<LocalTree::ProcessComponentFactory,
      //   Nodal::LocalTreeProcessComponentFactory>
      >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_addon_nodal::make_commands()
{
  using namespace Nodal;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Nodal::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_addon_nodal_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_nodal)
