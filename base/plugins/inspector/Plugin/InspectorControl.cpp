#include "InspectorControl.hpp"


InspectorControl::InspectorControl() :
    iscore::PluginControlInterface {"InspectorControl", nullptr},
    m_widgetList{new InspectorWidgetList{this}}
{
}

void InspectorControl::populateMenus(iscore::MenubarManager*)
{
}

InspectorWidgetList* InspectorControl::widgetList() const
{
    return m_widgetList;
}
