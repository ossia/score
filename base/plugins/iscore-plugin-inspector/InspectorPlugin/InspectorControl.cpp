#include "InspectorControl.hpp"


InspectorControl::InspectorControl(iscore::Application& app) :
    iscore::PluginControlInterface {app, "InspectorControl", nullptr},
    m_widgetList{new InspectorWidgetList{this}}
{
}

InspectorWidgetList* InspectorControl::widgetList() const
{
    return m_widgetList;
}
