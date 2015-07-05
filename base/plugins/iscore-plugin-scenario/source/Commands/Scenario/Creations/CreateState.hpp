#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class ScenarioModel;
class EventModel;
class StateModel;

namespace Scenario
{
namespace Command
{
class CreateState : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateState","CreateState")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateState, "ScenarioControl")

          CreateState(
            const ScenarioModel& scenario,
            const id_type<EventModel>& event,
            double stateY);

        CreateState(const ObjectPath &scenarioPath,
          const id_type<EventModel>& event,
          double stateY);

        const ObjectPath& scenarioPath() const
        { return m_path; }

        const double& endStateY() const
        { return m_stateY; }

        const id_type<StateModel>& createdState() const
        { return m_newState; }

        void undo() override;

        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        id_type<StateModel> m_newState;
        id_type<EventModel> m_event;
        double m_stateY{};
};
}
}
