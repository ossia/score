#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include "CreateState.hpp"
#include "Scenario/Commands/ScenarioCommandFactory.hpp"
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class DataStreamInput;
class DataStreamOutput;
class EventModel;
class StateModel;
class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

namespace Scenario
{
namespace Command
{
class CreateEvent_State final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateEvent_State, "Create an event and a state")
        public:

          CreateEvent_State(
            const Scenario::ScenarioModel& scenario,
            const Id<TimeNodeModel>& timeNode,
            double stateY);
        CreateEvent_State(
          const Path<Scenario::ScenarioModel>& scenario,
          const Id<TimeNodeModel>& timeNode,
          double stateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_newEvent; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Id<EventModel> m_newEvent;
        QString m_createdName;

        CreateState m_command;

        Id<TimeNodeModel> m_timeNode;
};
}
}
