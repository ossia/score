#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QString>

class QWidget;
namespace Process { class ProcessModel; }
namespace Scenario {
class ProcessModel;
}  // namespace Scenario
namespace iscore {
class Document;
}  // namespace iscore



class ScenarioInspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Scenario::ProcessModel>
{
    public:
        explicit ScenarioInspectorWidget(
                const Scenario::ProcessModel& object,
                QWidget* parent);
};
