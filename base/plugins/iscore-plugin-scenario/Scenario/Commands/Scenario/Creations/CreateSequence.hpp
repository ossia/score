#pragma once
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "CreateConstraint_State_Event_TimeNode.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class EventModel;
class StateModel;
class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
template <typename tag, typename impl> class id_base_t;

namespace Scenario
{
namespace Command
{
class CreateSequence final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateSequence,"Create a sequence")
    public:

        CreateSequence(
            const Scenario::ScenarioModel& scenario,
            const Id<StateModel>& startState,
            const TimeValue& date,
            double endStateY);

        CreateSequence(
            const Path<Scenario::ScenarioModel>& scenarioPath,
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
        { return m_command.createdTimeNode(); }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        CreateConstraint_State_Event_TimeNode m_command;
        InterpolateMacro m_interpolations;
        MessageNode m_stateData;
};
}
}
