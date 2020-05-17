#pragma once
#include <Process/ExpandMode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

#include <score/tools/std/HashMap.hpp>
#include <rapidjson/document.h>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;

namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT InsertContentInInterval final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      InsertContentInInterval,
      "Insert content in a interval")
public:
  InsertContentInInterval(
      const rapidjson::Value& sourceInterval,
      const IntervalModel& targetInterval,
      ExpandMode mode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  rapidjson::Document m_source;
  Path<IntervalModel> m_target;
  ExpandMode m_mode{ExpandMode::GrowShrink};

  score::hash_map<Id<Process::ProcessModel>, Id<Process::ProcessModel>>
      m_processIds;
};
}
}
