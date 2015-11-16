#include "InspectorPlugin.hpp"
#include "InspectorControl.hpp"

#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Inspector/InspectorWidgetList.hpp>

iscore_plugin_inspector::iscore_plugin_inspector() :
    QObject {},
        iscore::PanelFactory_QtInterface {}
{
}


QList<iscore::PanelFactory*> iscore_plugin_inspector::panels()
{
    return {new InspectorPanelFactory};
}

std::vector<FactoryListInterface*> iscore_plugin_inspector::factoryFamilies()
{
    return {new InspectorWidgetList};
}

