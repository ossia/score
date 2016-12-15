#include "iscore_plugin_midi.hpp"
#include <Midi/Inspector/MidiProcessInspector.hpp>
#include <Midi/MidiExecutor.hpp>
#include <Midi/MidiFactory.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>

#include <iscore_plugin_midi_commands_files.hpp>

iscore_plugin_midi::iscore_plugin_midi() : QObject{}
{
}

iscore_plugin_midi::~iscore_plugin_midi()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_midi::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<Process::ProcessModelFactory, Midi::ProcessFactory>, FW<Process::LayerFactory, Midi::LayerFactory>, FW<Engine::Execution::ProcessComponentFactory, Midi::Executor::ComponentFactory>, FW<Process::InspectorWidgetDelegateFactory, Midi::InspectorFactory>>(      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_midi::make_commands()
{
  using namespace Midi;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Midi::CommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_midi_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}

iscore::Version iscore_plugin_midi::version() const
{
  return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_midi::key() const
{
  return_uuid("0a964c0f-dd69-4e5a-9577-0ec5695690b0");
}
