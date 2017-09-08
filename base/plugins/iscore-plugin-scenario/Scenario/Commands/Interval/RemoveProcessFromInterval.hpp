#pragma once
#include <QByteArray>
#include <QPair>
#include <Scenario/Document/Interval/Slot.hpp>
#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

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
class RemoveProcessFromInterval final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RemoveProcessFromInterval,
      "Remove a process")
public:
  RemoveProcessFromInterval(
      const IntervalModel& cst,
      Id<Process::ProcessModel>
          processId);
  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_path;
  Id<Process::ProcessModel> m_processId;
  QByteArray m_serializedProcessData;

  Rack m_smallView;
  bool m_smallViewVisible{};
};
}
}
