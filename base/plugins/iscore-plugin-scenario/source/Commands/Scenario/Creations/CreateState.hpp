#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
class ScenarioModel;
class EventModel;
class StateModel;

namespace Scenario
{
namespace Command
{
class CreateState : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("CreateState","CreateState")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(CreateState, "ScenarioControl")

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

        void undo() override;

        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<ScenarioModel> m_path;

        Id<StateModel> m_newState;
        Id<EventModel> m_event;
        double m_stateY{};
};
}
}
