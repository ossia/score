#pragma once
#include <Process/TimeValue.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include "CreateConstraint_State_Event.hpp"
#include "Scenario/Commands/ScenarioCommandFactory.hpp"
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class EventModel;
class StateModel;
class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

namespace Scenario
{
namespace Command
{
class CreateConstraint_State_Event_TimeNode final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint_State_Event_TimeNode, "Create a constraint, a state, an event and a timenode")
        public:

          CreateConstraint_State_Event_TimeNode(
            const Scenario::ScenarioModel& scenario,
            const Id<StateModel>& startState,
            const TimeValue& date,
            double endStateY);

        CreateConstraint_State_Event_TimeNode(
          const Path<Scenario::ScenarioModel>& scenario,
          const Id<StateModel>& startState,
          const TimeValue& date,
          double endStateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_command.createdEvent(); }

        const Id<TimeNodeModel>& createdTimeNode() const
        { return m_newTimeNode; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Id<TimeNodeModel> m_newTimeNode;
        QString m_createdName;

        CreateConstraint_State_Event m_command;

        TimeValue m_date;
};
}
}
