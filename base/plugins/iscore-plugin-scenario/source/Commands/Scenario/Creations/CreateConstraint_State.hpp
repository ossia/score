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
            ScenarioModel& scenario,
            const id_type<DisplayedStateModel>& startState,
            const id_type<EventModel>& endEvent,
            double endStateY);

        const ObjectPath& scenarioPath() const
        { return m_createConstraint.scenarioPath(); }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        id_type<DisplayedStateModel> m_endStateId;
        CreateConstraint m_createConstraint;
        id_type<EventModel> m_endEvent;
        double m_endStateY{};
};
}
}
