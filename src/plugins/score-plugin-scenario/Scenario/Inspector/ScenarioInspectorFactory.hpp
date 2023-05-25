#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
namespace Scenario
{
class InspectorWidgetDelegateFactory final
    : public Process::InspectorWidgetDelegateFactory
{
  SCORE_CONCRETE("df1b9167-db40-4f88-8d52-b28e3ad1deee")
private:
  QWidget* makeProcess(
      const Process::ProcessModel& process, const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matchesProcess(const Process::ProcessModel& process) const override;

  void addButtons(
      const Process::ProcessModel&, const score::DocumentContext& doc,
      QBoxLayout* layout, QWidget* parent) const override;
};
}
