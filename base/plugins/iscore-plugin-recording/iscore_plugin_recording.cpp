#include <QString>
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Commands/RecordingCommandFactory.hpp>
#include <iscore/tools/ForEachType.hpp>
#include <iscore/tools/std/HashMap.hpp>

#include <Recording/RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp>
#include <Recording/RecordedMessages/Inspector/RecordedMessagesInspectorFactory.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcess.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessFactory.hpp>

#include "iscore_plugin_recording.hpp"
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore_plugin_recording_commands_files.hpp>

iscore_plugin_recording::iscore_plugin_recording() : QObject{}
{
}

iscore_plugin_recording::~iscore_plugin_recording()
{
}

iscore::GUIApplicationContextPlugin*
iscore_plugin_recording::make_applicationPlugin(
    const iscore::GUIApplicationContext& app)
{
  return new Recording::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_recording::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<Process::ProcessModelFactory, RecordedMessages::ProcessFactory>, FW<Process::LayerFactory, RecordedMessages::LayerFactory>, FW<Process::InspectorWidgetDelegateFactory, RecordedMessages::InspectorFactory>, FW<Engine::Execution::ProcessComponentFactory, RecordedMessages::Executor::ComponentFactory>>(
      ctx, key);
}

QStringList iscore_plugin_recording::required() const
{
  return {"Scenario", "Engine"};
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_recording::make_commands()
{
  using namespace Recording;
  using namespace RecordedMessages;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      RecordingCommandFactoryName(), CommandGeneratorMap{}};

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
