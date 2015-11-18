#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateConstraint_State_Event.hpp"
#include <Process/TimeValue.hpp>
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateConstraint_State_Event_TimeNode final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint_State_Event_TimeNode, "CreateConstraint_State_Event_TimeNode")
        public:

          CreateConstraint_State_Event_TimeNode(
            const ScenarioModel& scenario,
            const Id<StateModel>& startState,
            const TimeValue& date,
            double endStateY);

        CreateConstraint_State_Event_TimeNode(
          const Path<ScenarioModel>& scenario,
          const Id<StateModel>& startState,
          const TimeValue& date,
          double endStateY);

        const Path<ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

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
        QString m_createdName;

        CreateConstraint_State_Event m_command;

        TimeValue m_date;
};
}
}
