#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QString>

namespace Scenario
{
class ProcessModel;
} // namespace Scenario
namespace score
{
struct DocumentContext;
} // namespace score

class ScenarioInspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Scenario::ProcessModel>
{
public:
  explicit ScenarioInspectorWidget(
      const Scenario::ProcessModel& object,
      const score::DocumentContext& doc,
      QWidget* parent);
};
