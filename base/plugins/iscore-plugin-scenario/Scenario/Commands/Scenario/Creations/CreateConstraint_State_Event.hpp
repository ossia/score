#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include "CreateConstraint_State.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class ScenarioModel;
class EventModel;
class StateModel;
class TimeNodeModel;
class ConstraintModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateConstraint_State_Event final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint_State_Event, "Create a constraint, a state and an event")
        public:

          CreateConstraint_State_Event(
            const Scenario::ScenarioModel& scenario,
            Id<StateModel> startState,
            Id<TimeNodeModel> endTimeNode,
            double endStateY);

        CreateConstraint_State_Event(
          const Path<Scenario::ScenarioModel>& scenario,
          Id<StateModel> startState,
          Id<TimeNodeModel> endTimeNode,
          double endStateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const double& endStateY() const
        { return m_command.endStateY(); }

        const Id<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const Id<StateModel>& startState() const
        { return m_command.startState(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_newEvent; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Id<EventModel> m_newEvent;
        QString m_createdName;

        CreateConstraint_State m_command;

        Id<TimeNodeModel> m_endTimeNode;
};
}
}
