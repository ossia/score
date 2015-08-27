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
        ISCORE_COMMAND_DECL("CreateTimeNode_Event_State","CreateTimeNode_Event_State")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateTimeNode_Event_State, "ScenarioControl")

          CreateTimeNode_Event_State(
            const ScenarioModel& scenario,
            const TimeValue& date,
            double stateY);

        CreateTimeNode_Event_State(
          const ModelPath<ScenarioModel>& scenario,
          const TimeValue& date,
          double stateY);

        const ModelPath<ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

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
        TimeValue m_date;

        CreateEvent_State m_command;
};
}
}
