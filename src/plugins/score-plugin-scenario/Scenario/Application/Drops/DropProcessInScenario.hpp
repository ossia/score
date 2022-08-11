#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

class DropProcessInScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("9a094988-b05f-4e10-8e0d-56e8d46e084d")

public:
  DropProcessInScenario();
  void init();

private:
  bool
  drop(const Scenario::ScenarioPresenter&, QPointF pos, const QMimeData& mime) override;
};

}
