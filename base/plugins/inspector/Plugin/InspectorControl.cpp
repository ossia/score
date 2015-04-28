#include "InspectorControl.hpp"


InspectorControl::InspectorControl() :
    iscore::PluginControlInterface {"InspectorControl", nullptr},
    m_widgetList{new InspectorWidgetList{this}}
{
}

InspectorWidgetList* InspectorControl::widgetList() const
{
    return m_widgetList;
}
