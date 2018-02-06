#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
namespace Scenario
{
class IntervalModel;
}
namespace Scenario::Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT DuplicateInterval final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      DuplicateInterval,
      "Duplicate an interval")
public:
  DuplicateInterval(
      const Scenario::ProcessModel& parent,
      const IntervalModel& cst);
  ~DuplicateInterval();
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<IntervalModel>& intervalPath() const;


protected:
  void serializeImpl(DataStreamInput& s) const override;

  void deserializeImpl(DataStreamOutput& s) override;

private:
  CreateState m_cmdStart;
  CreateState m_cmdEnd;
  Path<IntervalModel> m_path;
  Id<Scenario::IntervalModel> m_createdId{};
};

}
