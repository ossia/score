#include "score_plugin_cliplauncher.hpp"

#include <ClipLauncher/CommandFactory.hpp>
#include <ClipLauncher/Commands/CellTriggerCommandFactory.hpp>
#include <ClipLauncher/Execution/ClipLauncherComponent.hpp>
#include <ClipLauncher/Inspector/CellInspectorFactory.hpp>
#include <ClipLauncher/Inspector/LaneInspectorFactory.hpp>
#include <ClipLauncher/Inspector/SceneInspectorFactory.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/View/CellDisplayedElementsProvider.hpp>
#include <ClipLauncher/View/LayerFactory.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <score_plugin_cliplauncher_commands_files.hpp>

score_plugin_cliplauncher::score_plugin_cliplauncher() { }
score_plugin_cliplauncher::~score_plugin_cliplauncher() { }

std::vector<score::InterfaceBase*> score_plugin_cliplauncher::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, ClipLauncher::ProcessFactory>,
      FW<Execution::ProcessComponentFactory,
         ClipLauncher::Execution::ClipLauncherComponentFactory>>(ctx, key);
}

std::vector<score::InterfaceBase*> score_plugin_cliplauncher::guiFactories(
    const score::GUIApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::GUIApplicationContext,
      FW<Process::LayerFactory, ClipLauncher::LayerFactory>,
      FW<Scenario::DisplayedElementsProvider,
         ClipLauncher::CellDisplayedElementsProvider>,
      FW<Scenario::Command::TriggerCommandFactory,
         ClipLauncher::CellTriggerCommandFactory>,
      FW<Inspector::InspectorWidgetFactory,
         ClipLauncher::CellInspectorFactory,
         ClipLauncher::LaneInspectorFactory,
         ClipLauncher::SceneInspectorFactory>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_cliplauncher::make_commands()
{
  using namespace ClipLauncher;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_cliplauncher_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_cliplauncher)
