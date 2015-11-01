#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateConstraint_State.hpp"
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateConstraint_State_Event final : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint_State_Event, "CreateConstraint_State_Event")
        public:

          CreateConstraint_State_Event(
            const ScenarioModel& scenario,
            const Id<StateModel>& startState,
            const Id<TimeNodeModel>& endTimeNode,
            double endStateY);

        CreateConstraint_State_Event(
          const Path<ScenarioModel>& scenario,
          const Id<StateModel>& startState,
          const Id<TimeNodeModel>& endTimeNode,
          double endStateY);

        const Path<ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const double& endStateY() const
        { return m_command.endStateY(); }

        const Id<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_newEvent; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Id<EventModel> m_newEvent;
        QString m_createdName;

        CreateConstraint_State m_command;

        Id<TimeNodeModel> m_endTimeNode;
};
}
}
