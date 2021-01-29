#include "score_plugin_lv2.hpp"

#include <LV2/EffectModel.hpp>
#include <LV2/Library.hpp>
#include <LV2/Window.hpp>
#include <Library/LibraryInterface.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <wobjectimpl.h>


score_plugin_lv2::score_plugin_lv2()
{
}

score_plugin_lv2::~score_plugin_lv2() { }


score::ApplicationPlugin*
score_plugin_lv2::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new LV2::ApplicationPlugin{app};
}


std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_lv2::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory,
         LV2::ProcessFactory
      >,
      FW<Process::LayerFactory,
         LV2::LayerFactory
      >,
      FW<Library::LibraryInterface,
         LV2::LibraryHandler
      >,
      FW<Execution::ProcessComponentFactory,
         LV2::ExecutorFactory
      >
  >(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_lv2)
