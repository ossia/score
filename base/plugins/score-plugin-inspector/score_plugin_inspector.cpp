// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_inspector.hpp"

#include <InspectorPlugin/Panel/InspectorPanelFactory.hpp>

#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/tools/ForEachType.hpp>

namespace score
{
class InterfaceListBase;
class PanelFactory;
} // namespace score

#include <Inspector/InspectorWidgetList.hpp>

score_plugin_inspector::score_plugin_inspector()
{
}

score_plugin_inspector::~score_plugin_inspector()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_inspector::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<score::PanelDelegateFactory, InspectorPanel::PanelDelegateFactory>>(
      ctx, key);
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_inspector::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase, Inspector::InspectorWidgetList>();
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_inspector)
