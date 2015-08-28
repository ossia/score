#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateConstraint_State_Event.hpp"
#include <ProcessInterface/TimeValue.hpp>
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateConstraint_State_Event_TimeNode : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateConstraint_State_Event_TimeNode","CreateConstraint_State_Event_TimeNode")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateConstraint_State_Event_TimeNode, "ScenarioControl")

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

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Id<TimeNodeModel> m_newTimeNode;
        QString m_createdName;

        CreateConstraint_State_Event m_command;

        TimeValue m_date;
};
}
}
