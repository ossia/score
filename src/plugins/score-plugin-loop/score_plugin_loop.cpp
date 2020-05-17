// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_loop.hpp"

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Loop/Commands/LoopCommandFactory.hpp>
#include <Loop/Inspector/LoopInspectorFactory.hpp>
#include <Loop/Inspector/LoopTriggerCommandFactory.hpp>
#include <Loop/LoopDisplayedElements.hpp>
#include <Loop/LoopExecution.hpp>
#include <Loop/LoopProcessFactory.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>
#include <Loop/LocalTree/LoopComponent.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score/tools/std/Optional.hpp>

#include <QMenu>

#include <score_plugin_loop_commands_files.hpp>
#include <string.h>

SCORE_DECLARE_ACTION(
    PutInLoop,
    "&Put in Loop",
    Loop,
    Qt::SHIFT + Qt::CTRL + Qt::Key_L)

namespace Loop
{
// TODO put in its own file
class ApplicationPlugin final : public QObject,
                                public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& ctx);
  ~ApplicationPlugin() override;
  score::GUIElements makeGUIElements() override;
};

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : GUIApplicationPlugin{ctx}
{
}

ApplicationPlugin::~ApplicationPlugin() {}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  auto& actions = e.actions;
  auto& base_menus = context.menus.get();

  auto putInLoop = new QAction{this};
  connect(putInLoop, &QAction::triggered, this, [this] {
    auto& ctx = currentDocument()->context();
    auto sm = Scenario::focusedScenarioModel(ctx);
    SCORE_ASSERT(sm);

    Loop::EncapsulateInLoop(*sm, ctx.commandStack);
  });

  actions.add<Actions::PutInLoop>(putInLoop);

  auto& scenariomodel_cond
      = context.actions.condition<Scenario::EnableWhenScenarioModelObject>();
  scenariomodel_cond.add<Actions::PutInLoop>();

  auto& object = base_menus.at(score::Menus::Object());
  object.menu()->addAction(putInLoop);
  return e;
}
}

score_plugin_loop::score_plugin_loop() = default;
score_plugin_loop::~score_plugin_loop() = default;

score::GUIApplicationPlugin* score_plugin_loop::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Loop::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_loop::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Execution::ProcessComponentFactory,
         Loop::RecreateOnPlay::ComponentFactory>,
      FW<Process::ProcessModelFactory, Loop::ProcessFactory>,
      FW<Process::LayerFactory, Loop::LayerFactory>,
      FW<TriggerCommandFactory, LoopTriggerCommandFactory>,
      FW<LocalTree::ProcessComponentFactory, LocalTree::LoopComponentFactory>,
      FW<Scenario::DisplayedElementsToolPaletteFactory,
         Loop::DisplayedElementsToolPaletteFactory>,
      FW<Scenario::DisplayedElementsProvider, Loop::DisplayedElementsProvider>,
      FW<Scenario::IntervalResizer, Loop::LoopIntervalResizer>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_loop::make_commands()
{
  using namespace Loop;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      LoopCommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_loop_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_loop)
