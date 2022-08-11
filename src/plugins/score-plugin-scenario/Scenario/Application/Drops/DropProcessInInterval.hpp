#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

#include <score_plugin_scenario_export.h>
namespace Scenario
{
namespace Command
{
class Macro;
}

class SCORE_PLUGIN_SCENARIO_EXPORT DropProcessInInterval final
    : public IntervalDropHandler
{
  SCORE_CONCRETE("08f5aec5-3a42-45c8-b3db-aa45a851dd09")

  bool drop(
      const score::DocumentContext& ctx, const Scenario::IntervalModel&, QPointF p,
      const QMimeData& mime) override;
};

}
