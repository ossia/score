#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

//! The copy carried by a drag from the processes' preset button, or nullptr
SCORE_PLUGIN_SCENARIO_EXPORT rapidjson::Value* draggedCopy(rapidjson::Value& json);
//! Such drags go to the layer handlers, not the preset ones
SCORE_PLUGIN_SCENARIO_EXPORT bool isProcessesDrag(const QMimeData& mime);
//! The current processes of this document that such a drag refers to
SCORE_PLUGIN_SCENARIO_EXPORT std::vector<const Process::ProcessModel*>
draggedProcesses(const rapidjson::Value& json, const score::DocumentContext& ctx);

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
