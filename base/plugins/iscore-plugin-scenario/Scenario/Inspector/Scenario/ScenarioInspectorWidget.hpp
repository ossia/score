#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QString>

class QWidget;
namespace Process { class ProcessModel; }
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
namespace iscore {
class Document;
}  // namespace iscore



class ScenarioInspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Scenario::ScenarioModel>
{
    public:
        explicit ScenarioInspectorWidget(
                const Scenario::ScenarioModel& object,
                QWidget* parent);
};
