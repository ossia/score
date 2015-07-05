#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateConstraint_State.hpp"
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateConstraint_State_Event : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateConstraint_State_Event","CreateConstraint_State_Event")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateConstraint_State_Event, "ScenarioControl")

          CreateConstraint_State_Event(
            const ScenarioModel& scenario,
            const id_type<DisplayedStateModel>& startState,
            const id_type<TimeNodeModel>& endTimeNode,
            double endStateY);

        const ObjectPath& scenarioPath() const
        { return m_command.scenarioPath(); }

        const double& endStateY() const
        { return m_command.endStateY(); }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        id_type<EventModel> m_newEvent;

        CreateConstraint_State m_command;

        id_type<TimeNodeModel> m_endTimeNode;
};
}
}
