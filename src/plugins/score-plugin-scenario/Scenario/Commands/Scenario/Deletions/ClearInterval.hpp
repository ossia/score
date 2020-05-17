#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Tools/dataStructures.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_plugin_scenario_export.h>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class IntervalModel;
namespace Command
{
/**
 * @brief The ClearInterval class
 *
 * Removes all the processes and the rackes of a interval.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ClearInterval final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ClearInterval, "Clear a interval")
public:
  ClearInterval(const IntervalModel& intervalPath);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  IntervalSaveData m_intervalSaveData;
  Dataflow::SerializedCables m_cables;
};
}
}
