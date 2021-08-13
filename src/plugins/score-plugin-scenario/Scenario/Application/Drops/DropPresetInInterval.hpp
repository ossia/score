#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

class DropPresetInInterval final : public IntervalDropHandler
{
  SCORE_CONCRETE("93d1dd9d-5923-4bc2-8c52-cbe0677a3202")

  bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF p,
      const QMimeData& mime) override;

public:
  static void perform(
      const IntervalModel& interval,
      const score::DocumentContext& doc,
      Scenario::Command::Macro& m,
      const QByteArray& presetData);
};

}
