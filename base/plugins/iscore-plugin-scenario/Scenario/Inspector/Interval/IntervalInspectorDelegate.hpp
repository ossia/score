#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <iscore_plugin_scenario_export.h>
#include <list>

class OngoingCommandDispatcher;
class QWidget;

namespace Scenario
{
class IntervalInspectorWidget;
class IntervalModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT IntervalInspectorDelegate
{
protected:
  const IntervalModel& m_model;

public:
  IntervalInspectorDelegate(const IntervalModel& cst) : m_model{cst}
  {
  }

  virtual ~IntervalInspectorDelegate();

  virtual void
  addWidgets_pre(std::vector<QWidget*>&, IntervalInspectorWidget* parent)
      = 0;
  virtual void
  addWidgets_post(std::vector<QWidget*>&, IntervalInspectorWidget* parent)
      = 0;

  virtual void updateElements() = 0;

  virtual void on_defaultDurationChanged(
      OngoingCommandDispatcher& dispatcher,
      const TimeVal& val,
      ExpandMode e) const = 0;
};
}
