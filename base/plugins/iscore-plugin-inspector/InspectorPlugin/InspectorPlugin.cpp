#include "InspectorPlugin.hpp"
#include "InspectorControl.hpp"

#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

iscore_plugin_inspector::iscore_plugin_inspector() :
    QObject {},
        iscore::PanelFactory_QtInterface {}
{
}


QList<iscore::PanelFactory*> iscore_plugin_inspector::panels()
{
    return {new InspectorPanelFactory};
}

std::vector<FactoryFamily> iscore_plugin_inspector::factoryFamilies()
{
    return {{InspectorWidgetFactory::staticFactoryName(),
             [&] (iscore::FactoryInterfaceBase* fact)
             {
                if(auto inspector = dynamic_cast<InspectorWidgetFactory*>(fact))
                    m_inspectorControl->widgetList()->registerFactory(inspector);
             }
           }};
}

PluginControlInterface* iscore_plugin_inspector::make_control(Presenter* pres)
{
    delete m_inspectorControl;
    m_inspectorControl = new InspectorControl{pres};
    return m_inspectorControl;
}
