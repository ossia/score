#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

class DropPresetInScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("bb137853-1bd9-4c38-a777-2d980771e567")

public:
  DropPresetInScenario();

private:
  bool
  drop(const Scenario::ScenarioPresenter&, QPointF pos, const QMimeData& mime) override;
};
}
