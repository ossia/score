#pragma once
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <list>

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

class OngoingCommandDispatcher;
class QWidget;

namespace Scenario
{
class IntervalInspectorWidget;
class IntervalModel;
class ScenarioIntervalInspectorDelegate final
    : public IntervalInspectorDelegate
{
public:
  ScenarioIntervalInspectorDelegate(const IntervalModel& cst);

  void updateElements() override;
  void addWidgets_pre(
      std::vector<QWidget*>& widgets,
      IntervalInspectorWidget* parent) override;
  void addWidgets_post(
      std::vector<QWidget*>& widgets,
      IntervalInspectorWidget* parent) override;

  void on_defaultDurationChanged(
      OngoingCommandDispatcher& dispatcher,
      const TimeVal& val,
      ExpandMode) const override;
};
}
