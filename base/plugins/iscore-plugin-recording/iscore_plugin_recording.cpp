// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Commands/RecordingCommandFactory.hpp>
#include <iscore/tools/ForEachType.hpp>
#include <iscore/tools/std/HashMap.hpp>

#include "iscore_plugin_recording.hpp"
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore_plugin_recording_commands_files.hpp>
#include <Scenario/iscore_plugin_scenario.hpp>
#include <iscore_plugin_engine.hpp>

iscore_plugin_recording::iscore_plugin_recording() : QObject{}
{
}

iscore_plugin_recording::~iscore_plugin_recording()
{
}

iscore::GUIApplicationPlugin*
iscore_plugin_recording::make_guiApplicationPlugin(
    const iscore::GUIApplicationContext& app)
{
  return new Recording::ApplicationPlugin{app};
}

auto iscore_plugin_recording::required() const
  -> std::vector<iscore::PluginKey>
{
    return {
      iscore_plugin_scenario::static_key(),
      iscore_plugin_engine::static_key()
    };
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_recording::make_commands()
{
  using namespace Recording;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      RecordingCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_recording_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}
