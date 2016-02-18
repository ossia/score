#include <RecordedMessages/Inspector/RecordedMessagesInspectorFactory.hpp>
#include <RecordedMessages/RecordedMessagesProcessFactory.hpp>
#include <RecordedMessages/RecordedMessagesProcess.hpp>
#include <unordered_map>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp"
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_recordedmessages.hpp"
#include <iscore_plugin_recordedmessages_commands_files.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

iscore_plugin_js::iscore_plugin_js() :
    QObject {}
{
}

iscore_plugin_js::~iscore_plugin_js()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_js::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<Process::ProcessFactory,
             RecordedMessages::ProcessFactory>,
        FW<ProcessInspectorWidgetDelegateFactory,
             RecordedMessages::InspectorFactory>,
        FW<RecreateOnPlay::ProcessComponentFactory,
             RecordedMessages::Executor::ProcessComponentFactory>
    >>(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_js::make_commands()
{
    using namespace RecordedMessages;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{
        RecordedMessages::CommandFactoryName(),
                CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_recordedmessages_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});


    return cmds;
}

iscore::Version iscore_plugin_js::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_js::key() const
{
    return "82124ca8-d4ca-4891-b77e-8f450b0377a4";
}
