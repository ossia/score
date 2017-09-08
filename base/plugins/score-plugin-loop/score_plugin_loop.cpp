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
score_plugin_loop::score_plugin_loop() : QObject{}
{
}

score_plugin_loop::~score_plugin_loop()
{
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
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      LoopCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <score_plugin_loop_commands.hpp>
      >;
  for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
