#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

class DropProcessInInterval final : public IntervalDropHandler
{
  SCORE_CONCRETE("08f5aec5-3a42-45c8-b3db-aa45a851dd09")

  bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF p,
      const QMimeData& mime) override;
};

}
