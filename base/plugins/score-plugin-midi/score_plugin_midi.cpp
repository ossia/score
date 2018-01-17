// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_midi.hpp"
#include <Midi/Inspector/MidiProcessInspector.hpp>
#include <Midi/MidiExecutor.hpp>
#include <Midi/MidiDrop.hpp>
#include <Midi/MidiFactory.hpp>

#include <score/plugins/customfactory/FactorySetup.hpp>

#include <score_plugin_midi_commands_files.hpp>

score_plugin_midi::score_plugin_midi() : QObject{}
{
}

score_plugin_midi::~score_plugin_midi()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_midi::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<score::ApplicationContext
      , FW<Process::ProcessModelFactory, Midi::ProcessFactory>
      , FW<Process::LayerFactory, Midi::LayerFactory>
      , FW<Engine::Execution::ProcessComponentFactory, Midi::Executor::ComponentFactory>
      , FW<Scenario::DropHandler, Midi::DropMidiInSenario>
      , FW<Inspector::InspectorWidgetFactory, Midi::InspectorFactory>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_midi::make_commands()
{
  using namespace Midi;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Midi::CommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <score_plugin_midi_commands.hpp>
      >;
  for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
