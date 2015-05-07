#include "InspectorPlugin.hpp"
#include "InspectorControl.hpp"

#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

InspectorPlugin::InspectorPlugin() :
    QObject {},
        iscore::PanelFactory_QtInterface {},
        m_inspectorControl {new InspectorControl}
{
}


QList<iscore::PanelFactory*> InspectorPlugin::panels()
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
