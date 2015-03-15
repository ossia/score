#include "InspectorControl.hpp"


InspectorControl::InspectorControl() :
    iscore::PluginControlInterface {"InspectorControl", nullptr},
    m_widgetList{new InspectorWidgetList{this}}
{
}

void InspectorControl::populateMenus(iscore::MenubarManager*)
{
}

void InspectorControl::populateToolbars()
{
}

InspectorWidgetList* InspectorControl::widgetList() const
{
    return m_widgetList;
}
