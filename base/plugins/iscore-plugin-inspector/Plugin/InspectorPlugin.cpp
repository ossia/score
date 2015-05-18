#include "InspectorPlugin.hpp"
#include "InspectorControl.hpp"

#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

iscore_plugin_inspector::iscore_plugin_inspector() :
    QObject {},
        iscore::PanelFactory_QtInterface {},
        m_inspectorControl {new InspectorControl}
{
}


QList<iscore::PanelFactory*> iscore_plugin_inspector::panels()
{
    return {new InspectorPanelFactory};
}

QVector<FactoryFamily> iscore_plugin_inspector::factoryFamilies()
{
    return {{"Inspector",
             [&] (iscore::FactoryInterface* fact)
             {
                m_inspectorControl->widgetList()->registerFactory(fact);
             }
           }};
}

PluginControlInterface* iscore_plugin_inspector::control()
{
    return m_inspectorControl;
}
