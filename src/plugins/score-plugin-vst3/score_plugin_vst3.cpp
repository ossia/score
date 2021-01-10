#include "score_plugin_vst3.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <score_plugin_vst3_commands_files.hpp>
#include <Vst3/EffectModel.hpp>
#include <Vst3/Executor.hpp>
#include <Vst3/Plugin.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
namespace vst3
{

inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"VST3"};
  return key;
}
}

score_plugin_vst3::score_plugin_vst3() { }

score_plugin_vst3::~score_plugin_vst3() { }

score::ApplicationPlugin* score_plugin_vst3::make_applicationPlugin(
    const score::ApplicationContext& ctx)
{
  return new vst3::ApplicationPlugin{ctx};
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_vst3::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext
      , FW<Process::ProcessModelFactory, vst3::VSTEffectFactory>
      , FW<Execution::ProcessComponentFactory, vst3::ExecutorFactory>
  >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_vst3::make_commands()
{
  using namespace vst3;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_vst3_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_vst3)
