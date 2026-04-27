#pragma once
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

#include <score_plugin_scenario_export.h>

namespace Sequence
{

class SCORE_PLUGIN_SCENARIO_EXPORT SequenceDropHandler final
    : public Scenario::IntervalDropHandler
{
  SCORE_CONCRETE("b2c4e8a1-3f71-4d9e-a0b5-6c2d7f8e4a3b")

public:
  bool drop(
      const score::DocumentContext& ctx, const Scenario::IntervalModel& interval,
      QPointF pos, const QMimeData& mime) override;
};

} // namespace Sequence
