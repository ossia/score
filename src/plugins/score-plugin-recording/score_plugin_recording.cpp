// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_recording.hpp"

#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Commands/RecordingCommandFactory.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/tools/std/HashMap.hpp>

#include <score_plugin_engine.hpp>
#include <score_plugin_recording_commands_files.hpp>
#include <score_plugin_scenario.hpp>

score_plugin_recording::score_plugin_recording() = default;
score_plugin_recording::~score_plugin_recording() = default;

score::GUIApplicationPlugin*
score_plugin_recording::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new Recording::ApplicationPlugin{app};
}

auto score_plugin_recording::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_scenario::static_key(), score_plugin_engine::static_key()};
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_recording::make_commands()
{
  using namespace Recording;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      RecordingCommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_recording_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_recording)
