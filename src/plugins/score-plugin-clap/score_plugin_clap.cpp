#include "score_plugin_clap.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <Clap/ApplicationPlugin.hpp>
#include <Clap/EffectModel.hpp>
#include <Clap/Executor.hpp>
#include <Clap/Library.hpp>
#include <Clap/Window.hpp>

#include <score_plugin_engine.hpp>
#include <score_plugin_library.hpp>

score_plugin_clap::score_plugin_clap() = default;
score_plugin_clap::~score_plugin_clap() = default;

score::GUIApplicationPlugin*
score_plugin_clap::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new Clap::ApplicationPlugin{app};
}

std::vector<score::InterfaceBase*> score_plugin_clap::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext, 
      FW<Process::ProcessModelFactory, Clap::ProcessFactory>,
      FW<Process::LayerFactory, Clap::EffectLayerFactory>,
      FW<Execution::ProcessComponentFactory, Clap::ExecutorFactory>,
      FW<Library::LibraryInterface, Clap::LibraryHandler>>(ctx, key);
}

std::vector<score::PluginKey> score_plugin_clap::required() const
{
  return {score_plugin_engine::static_key(), score_plugin_library::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_clap)
