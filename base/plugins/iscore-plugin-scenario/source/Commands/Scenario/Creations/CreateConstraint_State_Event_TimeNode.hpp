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

        const ObjectPath& scenarioPath() const;

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        id_type<TimeNodeModel> m_newTimeNode;

        CreateConstraint_State_Event m_command;

        TimeValue m_date;
};
}
}
