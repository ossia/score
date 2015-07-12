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
            const id_type<StateModel>& startState,
            const TimeValue& date,
            double endStateY);

        CreateConstraint_State_Event_TimeNode(
          const ObjectPath& scenario,
          const id_type<StateModel>& startState,
          const TimeValue& date,
          double endStateY);

        const ObjectPath& scenarioPath() const;

        const id_type<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const id_type<StateModel>& createdState() const
        { return m_command.createdState(); }

        const id_type<EventModel>& createdEvent() const
        { return m_command.createdEvent(); }

        const id_type<TimeNodeModel>& createdTimeNode() const
        { return m_newTimeNode; }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        id_type<TimeNodeModel> m_newTimeNode;
        QString m_createdName;

        CreateConstraint_State_Event m_command;

        TimeValue m_date;
};
}
}
