#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateConstraint.hpp"
class ScenarioModel;
namespace Scenario
{
namespace Command
{
class CreateConstraint_State : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateConstraint_State","CreateConstraint_State")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateConstraint_State, "ScenarioControl")

          CreateConstraint_State(
            const ScenarioModel& scenario,
            const id_type<StateModel>& startState,
            const id_type<EventModel>& endEvent,
            double endStateY);

        const ObjectPath& scenarioPath() const
        { return m_command.scenarioPath(); }

        const double& endStateY() const
        { return m_stateY; }

        const id_type<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const id_type<StateModel>& createdState() const
        { return m_newState; }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        id_type<StateModel> m_newState;
        CreateConstraint m_command;
        id_type<EventModel> m_endEvent;
        double m_stateY{};
};
}
}
