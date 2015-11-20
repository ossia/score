#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
class ScenarioModel;
class EventModel;
class StateModel;

namespace Scenario
{
namespace Command
{
class CreateState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateState, "CreateState")
        public:

        CreateState(
            const ScenarioModel& scenario,
            const Id<EventModel>& event,
            double stateY);

        CreateState(
                const Path<ScenarioModel> &scenarioPath,
                const Id<EventModel>& event,
                double stateY);

        const Path<ScenarioModel>& scenarioPath() const
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
        Path<ScenarioModel> m_path;

        Id<StateModel> m_newState;
        Id<EventModel> m_event;
        double m_stateY{};
};
}
}
