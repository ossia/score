// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/ScenarioModel.hpp>

#include "ScenarioInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>

ScenarioInspectorWidget::ScenarioInspectorWidget(
    const Scenario::ProcessModel& object,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
{
}
