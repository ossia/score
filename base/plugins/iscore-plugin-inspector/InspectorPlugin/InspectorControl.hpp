#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <memory>

class InspectorControl : public iscore::PluginControlInterface
{
    public:
        explicit InspectorControl(iscore::Presenter*);
        InspectorWidgetList* widgetList() const;

    private:
        InspectorWidgetList* m_widgetList{};
};
