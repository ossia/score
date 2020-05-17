#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

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
}
