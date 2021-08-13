#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

class DropLayerInScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("0eb96d95-3f5f-4e7a-b806-d03d0ac88b48")

public:
  DropLayerInScenario();

private:
  bool drop(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) override;
};

}
