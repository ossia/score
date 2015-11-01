#include "InspectorControl.hpp"


InspectorControl::InspectorControl(iscore::Presenter* pres) :
    iscore::PluginControlInterface {pres, "InspectorControl", nullptr},
    m_widgetList{new InspectorWidgetList{this}}
{
}

InspectorWidgetList* InspectorControl::widgetList() const
{
    return m_widgetList;
}
