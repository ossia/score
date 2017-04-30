#include "iscore_plugin_library.hpp"

#include <Library/Panel/LibraryPanelFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_library::iscore_plugin_library()
{
}

iscore_plugin_library::~iscore_plugin_library()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_library::guiFactories(
    const iscore::GUIApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<iscore::PanelDelegateFactory, Library::PanelDelegateFactory>>(
      ctx, key);
}
