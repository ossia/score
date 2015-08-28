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
            const Id<TimeNodeModel>& timeNode,
            double stateY);
        CreateEvent_State(
          const Path<ScenarioModel>& scenario,
          const Id<TimeNodeModel>& timeNode,
          double stateY);

        const Path<ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_newEvent; }

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Id<EventModel> m_newEvent;
        QString m_createdName;

        CreateState m_command;

        Id<TimeNodeModel> m_timeNode;
};
}
}
