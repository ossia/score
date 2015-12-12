#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "CreateConstraint.hpp"
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class EventModel;
class StateModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

namespace Scenario
{
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateConstraint_State final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint_State, "Create a constraint and a state")
        public:

          CreateConstraint_State(
            const Scenario::ScenarioModel& scenario,
            const Id<StateModel>& startState,
            const Id<EventModel>& endEvent,
            double endStateY);

        CreateConstraint_State(
          const Path<Scenario::ScenarioModel>& scenario,
          const Id<StateModel>& startState,
          const Id<EventModel>& endEvent,
          double endStateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const double& endStateY() const
        { return m_stateY; }

        const Id<ConstraintModel>& createdConstraint() const
        { return m_command.createdConstraint(); }

        const Id<StateModel>& createdState() const
        { return m_newState; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Id<StateModel> m_newState;
        CreateConstraint m_command;
        Id<EventModel> m_endEvent;
        double m_stateY{};
};
}
}
