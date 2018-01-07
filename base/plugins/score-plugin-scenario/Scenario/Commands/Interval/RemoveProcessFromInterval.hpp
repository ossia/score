#pragma once
#include <QByteArray>
#include <QPair>
#include <Scenario/Document/Interval/Slot.hpp>
#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/model/Identifier.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>
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
class RemoveProcessFromInterval final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RemoveProcessFromInterval,
      "Remove a process")
public:
  RemoveProcessFromInterval(
      const IntervalModel& cst,
      Id<Process::ProcessModel>
          processId);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_path;
  Id<Process::ProcessModel> m_processId;
  QByteArray m_serializedProcessData;
  Dataflow::SerializedCables m_cables;

  Rack m_smallView;
  bool m_smallViewVisible{};
};
}
}
