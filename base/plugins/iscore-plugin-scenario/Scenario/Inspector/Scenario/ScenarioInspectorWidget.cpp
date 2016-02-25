#include <Scenario/Process/ScenarioModel.hpp>
#include <QBoxLayout>

#include <QPushButton>
#include <list>

#include <Inspector/InspectorWidgetBase.hpp>
#include "ScenarioInspectorWidget.hpp"

class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

ScenarioInspectorWidget::ScenarioInspectorWidget(
        const Scenario::ScenarioModel& object,
        QWidget* parent) :
    InspectorWidgetDelegate_T {object, parent}
{
}
