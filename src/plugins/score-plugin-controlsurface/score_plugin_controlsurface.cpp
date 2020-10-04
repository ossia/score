#include "score_plugin_controlsurface.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <ControlSurface/CommandFactory.hpp>
#include <ControlSurface/Executor.hpp>
#include <ControlSurface/Layer.hpp>
#include <ControlSurface/LocalTree.hpp>
#include <ControlSurface/Process.hpp>
#include <score_plugin_controlsurface_commands_files.hpp>

score_plugin_controlsurface::score_plugin_controlsurface() { }

score_plugin_controlsurface::~score_plugin_controlsurface() { }

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_controlsurface::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, ControlSurface::ProcessFactory>,
      FW<Process::LayerFactory, ControlSurface::LayerFactory>,
      FW<Execution::ProcessComponentFactory, ControlSurface::ProcessExecutorComponentFactory>,
      FW<LocalTree::ProcessComponentFactory, ControlSurface::LocalTreeProcessComponentFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_controlsurface::make_commands()
{
  using namespace ControlSurface;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_controlsurface_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_controlsurface)
