#pragma once

#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class TimeNodeModel;
namespace Command
{
class SplitTimeNode final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SplitTimeNode, "Split a timenode")
public:
  SplitTimeNode(
      Path<TimeNodeModel>&& path, QVector<Id<EventModel>> eventsInNewTimeNode);
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeNodeModel> m_path;
  QVector<Id<EventModel>> m_eventsInNewTimeNode;

  Id<TimeNodeModel> m_originalTimeNodeId;
  Id<TimeNodeModel> m_newTimeNodeId;
};
}
}
