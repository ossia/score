#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}
class DropLayerInInterval final : public IntervalDropHandler
{
  SCORE_CONCRETE("9df2eac6-6680-43cc-9634-60324416ba04")

  bool drop(
      const score::DocumentContext& ctx, const Scenario::IntervalModel&, QPointF p,
      const QMimeData& mime) override;

public:
  static void perform(
      const IntervalModel& interval, const score::DocumentContext& doc,
      Scenario::Command::Macro& m, const rapidjson::Document& json);
};
}
