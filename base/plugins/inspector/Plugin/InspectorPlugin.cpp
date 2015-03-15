#include "InspectorPlugin.hpp"
#include "InspectorControl.hpp"
#include <Inspector/InspectorWidgetList.hpp>

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

QStringList InspectorPlugin::panel_list() const
{
    return {"Inspector Panel"};
}

PanelFactoryInterface* InspectorPlugin::panel_make(QString name)
{
    if(name == "Inspector Panel")
    {
        return new InspectorPanelFactory;
    }

    return nullptr;
}

QVector<FactoryFamily> InspectorPlugin::factoryFamilies_make()
{
    return {{"Inspector",
             [&] (iscore::FactoryInterface* fact)
             {
                m_inspectorControl->widgetList()->registerFactory(fact);
             }
           }};
}

PluginControlInterface* InspectorPlugin::control_make()
{
    return m_inspectorControl;
}
