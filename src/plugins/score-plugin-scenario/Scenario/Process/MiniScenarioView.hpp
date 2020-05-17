#pragma once
#include <Process/LayerView.hpp>
#include <Process/TimeValue.hpp>

#include <nano_observer.hpp>

namespace Scenario
{
class ProcessModel;
class IntervalModel;
class MiniScenarioView final : public QObject, public Process::MiniLayer, public Nano::Observer
{
public:
  MiniScenarioView(const Scenario::ProcessModel& sc, QGraphicsItem* p);

private:
  void on_elementChanged(const IntervalModel&) { update(); }

  void paint_impl(QPainter*) const override;
  const Scenario::ProcessModel& m_scenario;
};
}
