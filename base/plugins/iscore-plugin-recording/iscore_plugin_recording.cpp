#include <QString>
#include <unordered_map>
#include <iscore/tools/ForEachType.hpp>
#include <Recording/Commands/RecordingCommandFactory.hpp>
#include <Recording/ApplicationPlugin.hpp>

#include <Recording/RecordedMessages/Inspector/RecordedMessagesInspectorFactory.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessFactory.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcess.hpp>
#include <Recording/RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp>

#include "iscore_plugin_recording.hpp"
#include <iscore_plugin_recording_commands_files.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_recording::iscore_plugin_recording() :
    QObject {}
{
}

iscore_plugin_recording::~iscore_plugin_recording()
{

}

iscore::GUIApplicationContextPlugin* iscore_plugin_recording::make_applicationPlugin(
        const iscore::GUIApplicationContext& app)
{
    return new RecordingApplicationPlugin {app};
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase> > iscore_plugin_recording::factories(const iscore::ApplicationContext& ctx, const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<Process::ProcessFactory,
               RecordedMessages::ProcessFactory>,
            FW<Process::InspectorWidgetDelegateFactory,
               RecordedMessages::InspectorFactory>,
            FW<RecreateOnPlay::ProcessComponentFactory,
               RecordedMessages::Executor::ComponentFactory>
            >>(ctx, key);
}

QStringList iscore_plugin_recording::required() const
{
    return {"Scenario", "OSSIA"};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_recording::make_commands()
{
    using namespace Recording;
    using namespace RecordedMessages;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{RecordingCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_recording_commands.hpp>
    >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}

iscore::Version iscore_plugin_recording::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_recording::key() const
{
    return_uuid("659ba25e-97e5-40d9-8db8-f7a8537035ad");
}
