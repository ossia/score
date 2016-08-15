#include "iscore_plugin_midi.hpp"
#include <Midi/MidiFactory.hpp>
#include <Midi/MidiExecutor.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>

#include <iscore_plugin_midi_commands_files.hpp>

iscore_plugin_midi::iscore_plugin_midi() :
    QObject {}
{
}

iscore_plugin_midi::~iscore_plugin_midi()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_midi::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
                FW<Process::ProcessFactory, Midi::ProcessFactory>,
                FW<Engine::Execution::ProcessComponentFactory, Midi::Executor::ComponentFactory>
            >>(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_midi::make_commands()
{
    using namespace Midi;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{Midi::CommandFactoryName(), CommandGeneratorMap{}};

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
