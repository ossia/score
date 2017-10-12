// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Loop/Commands/LoopCommandFactory.hpp>
#include <Loop/Inspector/LoopInspectorFactory.hpp>
#include <Loop/Inspector/LoopTriggerCommandFactory.hpp>
#include <Loop/LoopProcessFactory.hpp>
#include <Process/Process.hpp>
#include <score/tools/std/Optional.hpp>
#include <string.h>
#include <score/tools/std/HashMap.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include "score_plugin_loop.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Loop/LoopDisplayedElements.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/model/Identifier.hpp>
#include <score_plugin_loop_commands_files.hpp>

#include <score/plugins/customfactory/FactorySetup.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

SCORE_DECLARE_ACTION(
    PutInLoop, "&Put in Loop", Loop, Qt::SHIFT + Qt::CTRL + Qt::Key_L)

namespace Loop {
class ApplicationPlugin
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& ctx):
    GUIApplicationPlugin{ctx}
  {
    m_putInLoop = new QAction{this};
    connect(m_putInLoop, &QAction::triggered, [this] {
      auto& ctx = currentDocument()->context();
      auto sm = Scenario::focusedScenarioModel(ctx);
      SCORE_ASSERT(sm);

      Loop::EncapsulateInLoop(*sm, ctx.commandStack);
    });
  }

  score::GUIElements makeGUIElements() override
  {
    score::GUIElements e;
    auto& actions = e.actions;
    auto& base_menus = context.menus.get();

    actions.add<Actions::PutInLoop>(m_putInLoop);

    auto& scenariomodel_cond
        = context.actions.condition<Scenario::EnableWhenScenarioModelObject>();
    scenariomodel_cond.add<Actions::PutInLoop>();

    auto& object = base_menus.at(score::Menus::Object());
    object.menu()->addAction(m_putInLoop);
    return e;
  }

private:
  QAction* m_putInLoop{};
};
}

score_plugin_loop::score_plugin_loop() : QObject{}
{
}

score_plugin_loop::~score_plugin_loop()
{
}

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
  return instantiate_factories<score::ApplicationContext,
      FW<Process::ProcessModelFactory, Loop::ProcessFactory>
      , FW<Process::LayerFactory, Loop::LayerFactory>
      , FW<Inspector::InspectorWidgetFactory, Loop::InspectorFactory>
      , FW<IntervalInspectorDelegateFactory, Loop::IntervalInspectorDelegateFactory>
      , FW<TriggerCommandFactory, LoopTriggerCommandFactory>
      , FW<Scenario::DisplayedElementsToolPaletteFactory, Loop::DisplayedElementsToolPaletteFactory>
      , FW<Scenario::DisplayedElementsProvider, Loop::DisplayedElementsProvider>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_loop::make_commands()
{
  using namespace Loop;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      LoopCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <score_plugin_loop_commands.hpp>
      >;
  for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
