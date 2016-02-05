#include "iscore_plugin_inspector.hpp"
#include "Panel/InspectorPanelFactory.hpp"
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/tools/ForEachType.hpp>

namespace iscore {
class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

#include <Inspector/InspectorWidgetList.hpp>

iscore_plugin_inspector::iscore_plugin_inspector() :
    QObject {},
        iscore::PanelFactory_QtInterface {}
{
}


std::vector<iscore::PanelFactory*> iscore_plugin_inspector::panels()
{
    return {new InspectorPanel::InspectorPanelFactory};
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_inspector::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Inspector::InspectorWidgetList>();
}

int32_t iscore_plugin_inspector::version() const
{
    return 1;
}

UuidKey<iscore::Plugin> iscore_plugin_inspector::key() const
{
    return "0ed1520f-e120-458e-a5a8-b3f05f3b6b6c";
}
