// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioInspectorWidget.hpp"

#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

ScenarioInspectorWidget::ScenarioInspectorWidget(
    const Scenario::ProcessModel& object, const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
{
}
