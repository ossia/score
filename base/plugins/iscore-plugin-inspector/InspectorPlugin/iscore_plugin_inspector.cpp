#include "iscore_plugin_inspector.hpp"
#include "Panel/InspectorPanelFactory.hpp"
#include <iscore/tools/ForEachType.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

namespace iscore {
class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

#include <Inspector/InspectorWidgetList.hpp>

iscore_plugin_inspector::iscore_plugin_inspector()
{
}

iscore_plugin_inspector::~iscore_plugin_inspector()
{

}


std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_inspector::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
              FW<iscore::PanelDelegateFactory,
                InspectorPanel::PanelDelegateFactory>
            >
    >(ctx, key);
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_inspector::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Inspector::InspectorWidgetList>();
}

iscore::Version iscore_plugin_inspector::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_inspector::key() const
{
    return "0ed1520f-e120-458e-a5a8-b3f05f3b6b6c";
}
