// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_midi.hpp"

#include <Midi/Inspector/MidiProcessInspector.hpp>
#include <Midi/MidiDrop.hpp>
#include <Midi/MidiExecutor.hpp>
#include <Midi/MidiFactory.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <Patternist/PatternExecutor.hpp>
#include <Patternist/PatternFactory.hpp>
#include <Patternist/PatternInspector.hpp>
#include <score_plugin_midi_commands_files.hpp>

score_plugin_midi::score_plugin_midi() = default;
score_plugin_midi::~score_plugin_midi() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_midi::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Midi::ProcessFactory, Patternist::ProcessFactory>,
      FW<Process::LayerFactory, Midi::LayerFactory, Patternist::LayerFactory>,
      FW<Execution::ProcessComponentFactory,
         Midi::Executor::ComponentFactory,
         Patternist::ExecutorFactory>,
      FW<Process::ProcessDropHandler, Midi::DropHandler>,
      FW<Inspector::InspectorWidgetFactory, Midi::InspectorFactory, Patternist::InspectorFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_midi::make_commands()
{
  using namespace Midi;
  using namespace Patternist;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Midi::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_midi_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_midi)
