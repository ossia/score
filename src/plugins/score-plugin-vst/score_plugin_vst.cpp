#include "score_plugin_vst.hpp"

#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Executor.hpp>
#include <Vst/Library.hpp>
#include <Vst/Settings.hpp>
#include <Vst/Widgets.hpp>
#include <Vst/Window.hpp>
#include <Library/LibraryInterface.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <score_plugin_vst_commands_files.hpp>
#include <wobjectimpl.h>

namespace vst
{
using LayerFactory = Process::EffectLayerFactory_T<Model, EffectItem, Window>;
}

score_plugin_vst::score_plugin_vst()
{
}

score_plugin_vst::~score_plugin_vst() { }

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_vst::make_commands()
{
  using namespace vst;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      vst::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_vst_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::ApplicationPlugin*
score_plugin_vst::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new vst::ApplicationPlugin{app};
}

score::GUIApplicationPlugin*
score_plugin_vst::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new vst::GUIApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_vst::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory,
        vst::VSTEffectFactory
      >,
      FW<Process::LayerFactory,
        vst::LayerFactory
      >,
      FW<Library::LibraryInterface,
        vst::LibraryHandler
      >,
      FW<Process::PortFactory,
        vst::ControlPortFactory
      >,
      FW<Execution::ProcessComponentFactory,
         vst::ExecutorFactory
      >,
      FW<Media::Settings::PluginSettingsTab,
         vst::SettingsWidget
      >
      >(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_vst)
