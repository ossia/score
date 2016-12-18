#include "iscore_plugin_inspector.hpp"
#include "Panel/InspectorPanelFactory.hpp"
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/tools/ForEachType.hpp>

namespace iscore
{
class InterfaceListBase;
class PanelFactory;
} // namespace iscore

#include <Inspector/InspectorWidgetList.hpp>

iscore_plugin_inspector::iscore_plugin_inspector()
{
}

iscore_plugin_inspector::~iscore_plugin_inspector()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_inspector::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<iscore::PanelDelegateFactory, InspectorPanel::PanelDelegateFactory>>(
      ctx, key);
}

std::vector<std::unique_ptr<iscore::InterfaceListBase>>
iscore_plugin_inspector::factoryFamilies()
{
  return make_ptr_vector<iscore::InterfaceListBase, Inspector::InspectorWidgetList>();
}
