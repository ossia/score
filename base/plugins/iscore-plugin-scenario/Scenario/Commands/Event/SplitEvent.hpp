#pragma once

#include <QString>
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
class ProcessModel;
class EventModel;
class StateModel;

namespace Command
{
class SplitEvent final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SplitEvent, "Split an event")

public:
  SplitEvent(
      const Path<Scenario::ProcessModel>& scenario,
      Id<EventModel>
          event,
      QVector<Id<StateModel>>
          movingstates);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_scenarioPath;

  Id<EventModel> m_originalEvent;
  Id<EventModel> m_newEvent;
  QString m_createdName;
  QVector<Id<StateModel>> m_movingStates;
};
}
}
