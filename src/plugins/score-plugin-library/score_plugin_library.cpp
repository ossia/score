// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_library.hpp"

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/Panel/LibraryPanelFactory.hpp>

#include <score/plugins/FactorySetup.hpp>

score_plugin_library::score_plugin_library() { }

score_plugin_library::~score_plugin_library() { }

std::vector<std::unique_ptr<score::InterfaceListBase>> score_plugin_library::factoryFamilies()
{
  return make_ptr_vector<score::InterfaceListBase, Library::LibraryInterfaceList>();
}
std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_library::guiFactories(
    const score::GUIApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<score::PanelDelegateFactory,
         Library::UserPanelFactory,
         Library::ProjectPanelFactory,
         Library::ProcessPanelFactory>,
      FW<Library::LibraryInterface, Library::LibraryDocumentLoader>,
      FW<score::SettingsDelegateFactory, Library::Settings::Factory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_library)
