#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>


class StateModel;
class ScenarioModel;

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, only vertical move is allowed (new state on an existing event)
 */
namespace Scenario
{
    namespace Command
    {

        class MoveNewState : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("MoveNewState", "MoveNewState")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(MoveNewState, "ScenarioControl")
            MoveNewState(
                ModelPath<ScenarioModel>&& scenarioPath,
                const id_type<StateModel>& stateId,
                const double y);

              virtual void undo() override;
              virtual void redo() override;

              void update(
                      const ModelPath<ScenarioModel>&,
                      const id_type<StateModel>&,
                      const double y)
              {
                  m_y = y;
              }

              const ModelPath<ScenarioModel>& path() const
              { return m_path; }

            protected:
              virtual void serializeImpl(QDataStream&) const override;
              virtual void deserializeImpl(QDataStream&) override;

            private:
              ModelPath<ScenarioModel> m_path;
              id_type<StateModel> m_stateId;
              double m_y{}, m_oldy{};
        };
    }
}
