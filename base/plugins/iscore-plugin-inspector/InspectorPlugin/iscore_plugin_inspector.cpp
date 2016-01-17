#include "iscore_plugin_inspector.hpp"
#include "Panel/InspectorPanelFactory.hpp"
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/tools/ForEachType.hpp>

namespace iscore {
class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

using namespace iscore;

#include <Inspector/InspectorWidgetList.hpp>

iscore_plugin_inspector::iscore_plugin_inspector() :
    QObject {},
        iscore::PanelFactory_QtInterface {}
{
}


std::vector<iscore::PanelFactory*> iscore_plugin_inspector::panels()
{
    return {new InspectorPanelFactory};
}

std::vector<std::unique_ptr<FactoryListInterface>> iscore_plugin_inspector::factoryFamilies()
{
    return make_ptr_vector<FactoryListInterface,
            Inspector::InspectorWidgetList>();
}

