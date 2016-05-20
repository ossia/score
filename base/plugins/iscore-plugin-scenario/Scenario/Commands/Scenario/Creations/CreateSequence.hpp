#pragma once
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "CreateConstraint_State_Event_TimeNode.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class ConstraintModel;
class StateModel;
class TimeNodeModel;
class ScenarioModel;
namespace Command
{
class CreateSequence final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateSequence,"Create a sequence")
    public:

        CreateSequence(
            const Scenario::ScenarioModel& scenario,
            Id<StateModel> startState,
            TimeValue date,
            double endStateY);

        CreateSequence(
            const Path<Scenario::ScenarioModel>& scenarioPath,
            Id<StateModel> startState,
            TimeValue date,
            double endStateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const Id<StateModel>& startState() const
        { return m_command.startState(); }

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
        AddMultipleProcessesToConstraintMacro m_interpolations;
        Process::MessageNode m_stateData;
        int m_addedProcessCount{};
};
}
}
