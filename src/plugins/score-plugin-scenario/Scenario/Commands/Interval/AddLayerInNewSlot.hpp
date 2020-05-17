#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

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
/**
 * @brief The AddLayerInNewSlot class
 */
class SCORE_PLUGIN_SCENARIO_EXPORT AddLayerInNewSlot final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddLayerInNewSlot, "Add a new layer")
public:
  AddLayerInNewSlot(Path<IntervalModel>&& intervalPath, Id<Process::ProcessModel> process);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  Id<Process::ProcessModel> processId() const { return m_processId; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_path;
  Id<Process::ProcessModel> m_processId{};
};
}
}
