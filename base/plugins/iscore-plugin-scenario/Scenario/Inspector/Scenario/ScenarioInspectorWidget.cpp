#include <Scenario/Process/ScenarioModel.hpp>

#include <Inspector/InspectorWidgetBase.hpp>
#include "ScenarioInspectorWidget.hpp"

ScenarioInspectorWidget::ScenarioInspectorWidget(
        const Scenario::ProcessModel& object,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    InspectorWidgetDelegate_T {object, parent}
{
}
