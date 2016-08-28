#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QString>

namespace Scenario {
class ProcessModel;
}  // namespace Scenario
namespace iscore {
struct DocumentContext;
}  // namespace iscore

class ScenarioInspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Scenario::ProcessModel>
{
    public:
        explicit ScenarioInspectorWidget(
                const Scenario::ProcessModel& object,
                const iscore::DocumentContext& doc,
                QWidget* parent);
};
