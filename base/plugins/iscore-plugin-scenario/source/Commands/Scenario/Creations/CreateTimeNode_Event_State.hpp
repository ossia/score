#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateEvent_State.hpp"
#include <ProcessInterface/TimeValue.hpp>
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateTimeNode_Event_State : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateTimeNode_Event_State,"CreateTimeNode_Event_State")
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
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Id<TimeNodeModel> m_newTimeNode;
        TimeValue m_date;

        CreateEvent_State m_command;
};
}
}
