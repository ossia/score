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

#include <QDebug>

#include <wobjectimpl.h>

static bool has_libsuil = false;
score_plugin_lv2::score_plugin_lv2()
{
  try
  {
    const libsuil& suil = libsuil::instance();
    has_libsuil = bool(suil.init);
  }
  catch(...)
  {
    has_libsuil = false;
    qDebug() << "Warning: libsuil-0.so.0 not found. LV2 will be disabled.";
  }
}

score_plugin_lv2::~score_plugin_lv2() { }

score::ApplicationPlugin*
score_plugin_lv2::make_applicationPlugin(const score::ApplicationContext& app)
{
  if(!has_libsuil)
    return nullptr;
  return new LV2::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_lv2::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  if(!has_libsuil)
    return {};
  return instantiate_factories<
      score::ApplicationContext, FW<Process::ProcessModelFactory, LV2::ProcessFactory>,
      FW<Process::LayerFactory, LV2::LayerFactory>,
      FW<Library::LibraryInterface, LV2::LibraryHandler>,
      FW<Execution::ProcessComponentFactory, LV2::ExecutorFactory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_lv2)
