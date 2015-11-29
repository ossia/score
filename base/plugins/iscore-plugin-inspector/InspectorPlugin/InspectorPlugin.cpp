#include "InspectorPlugin.hpp"
#include "Panel/InspectorPanelFactory.hpp"
#include "iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp"

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

std::vector<FactoryListInterface*> iscore_plugin_inspector::factoryFamilies()
{
    return {new InspectorWidgetList};
}

