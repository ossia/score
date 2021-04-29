#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
namespace Dataflow
{
class DropPortInScenario final : public Scenario::GhostIntervalDropHandler
{
  SCORE_CONCRETE("b71dd84e-e242-4451-bab5-970215c6b120")

public:
  DropPortInScenario();

private:
  bool canDrop(const QMimeData& mime) const noexcept override;
  bool drop(const Scenario::ScenarioPresenter&, QPointF pos, const QMimeData& mime) override;
};

/**
 * @brief What happens when a port is dropped to an interval
 */
class DropPortInInterval final : public Scenario::IntervalDropHandler
{
  SCORE_CONCRETE("30147c87-2dfb-458d-9474-b0ee46897b51")

  bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF p,
      const QMimeData& mime) override;
};

}
