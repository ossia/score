#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateConstraint_State_Event_TimeNode.hpp"
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/State/MessageNode.hpp>
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateSequence : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("CreateSequence","CreateSequence")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(CreateSequence, "ScenarioControl")

        CreateSequence(
            const ScenarioModel& scenario,
            const Id<StateModel>& startState,
            const TimeValue& date,
            double endStateY);

        CreateSequence(
            const Path<ScenarioModel>& scenarioPath,
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
        { return m_command.createdTimeNode(); }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        CreateConstraint_State_Event_TimeNode m_command;
        MessageNode m_stateData;
};
}
}
