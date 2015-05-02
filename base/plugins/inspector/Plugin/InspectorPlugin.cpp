#include "InspectorPlugin.hpp"
#include "InspectorControl.hpp"

#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

InspectorPlugin::InspectorPlugin() :
    QObject {},
        iscore::PanelFactoryInterface_QtInterface {},
        m_inspectorControl {new InspectorControl}
{
    setObjectName("InspectorPlugin");
}


QList<iscore::PanelFactoryInterface*> InspectorPlugin::panels()
{
    return {new InspectorPanelFactory};
}

QVector<FactoryFamily> InspectorPlugin::factoryFamilies()
{
    return {{"Inspector",
             [&] (iscore::FactoryInterface* fact)
             {
                m_inspectorControl->widgetList()->registerFactory(fact);
             }
           }};
}

PluginControlInterface* InspectorPlugin::control()
{
    return m_inspectorControl;
}
