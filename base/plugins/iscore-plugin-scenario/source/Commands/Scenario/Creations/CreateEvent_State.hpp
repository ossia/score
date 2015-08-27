#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "CreateState.hpp"
class TimeNodeModel;
namespace Scenario
{
namespace Command
{
class CreateEvent_State : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateEvent_State","CreateEvent_State")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateEvent_State, "ScenarioControl")

          CreateEvent_State(
            const ScenarioModel& scenario,
            const id_type<TimeNodeModel>& timeNode,
            double stateY);
        CreateEvent_State(
          const ModelPath<ScenarioModel>& scenario,
          const id_type<TimeNodeModel>& timeNode,
          double stateY);

        const ModelPath<ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const id_type<StateModel>& createdState() const
        { return m_command.createdState(); }

        const id_type<EventModel>& createdEvent() const
        { return m_newEvent; }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        id_type<EventModel> m_newEvent;
        QString m_createdName;

        CreateState m_command;

        id_type<TimeNodeModel> m_timeNode;
};
}
}
