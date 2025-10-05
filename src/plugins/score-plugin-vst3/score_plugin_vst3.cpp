#include "score_plugin_vst3.hpp"

#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/EffectModel.hpp>
#include <Vst3/Executor.hpp>
#include <Vst3/Library.hpp>
#include <Vst3/Plugin.hpp>
#include <Vst3/UI/Window.hpp>
#include <Vst3/Widgets.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_vst3_commands_files.hpp>

namespace vst3
{
using LayerFactory = Process::EffectLayerFactory_T<Model, Window>;
}
score_plugin_vst3::score_plugin_vst3() { }

score_plugin_vst3::~score_plugin_vst3() { }

score::ApplicationPlugin*
score_plugin_vst3::make_applicationPlugin(const score::ApplicationContext& ctx)
{
  return new vst3::ApplicationPlugin{ctx};
}

std::vector<score::InterfaceBase*> score_plugin_vst3::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, vst3::VSTEffectFactory>,
      FW<Execution::ProcessComponentFactory, vst3::ExecutorFactory>,
      FW<Library::LibraryInterface, vst3::LibraryHandler>,
      FW<Process::PortFactory, vst3::VSTControlPortFactory>,
      FW<Process::LayerFactory, vst3::LayerFactory>>(ctx, key);
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
