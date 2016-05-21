#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ScenarioModel;
class EventModel;
class StateModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateState, "Create a state")
        public:

        CreateState(
            const Scenario::ScenarioModel& scenario,
            Id<EventModel> event,
            double stateY);

        CreateState(
                const Path<Scenario::ScenarioModel> &scenarioPath,
                Id<EventModel> event,
                double stateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_path; }

        const double& endStateY() const
        { return m_stateY; }

        const Id<StateModel>& createdState() const
        { return m_newState; }

        void undo() const override;

        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<Scenario::ScenarioModel> m_path;

        Id<StateModel> m_newState;
        Id<EventModel> m_event;
        double m_stateY{};
};
}
}
