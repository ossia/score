#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateEvent_State.hpp"
#include <Process/TimeValue.hpp>
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateTimeNode_Event_State final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateTimeNode_Event_State,"Create a timenode, an event and a state")
        public:

          CreateTimeNode_Event_State(
            const ScenarioModel& scenario,
            const TimeValue& date,
            double stateY);

        CreateTimeNode_Event_State(
          const Path<ScenarioModel>& scenario,
          const TimeValue& date,
          double stateY);

        const Path<ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_command.createdEvent(); }

        const Id<TimeNodeModel>& createdTimeNode() const
        { return m_newTimeNode; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Id<TimeNodeModel> m_newTimeNode;
        TimeValue m_date;

        CreateEvent_State m_command;
};
}
}
