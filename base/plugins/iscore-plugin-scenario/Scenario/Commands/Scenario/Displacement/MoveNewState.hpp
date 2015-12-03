#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class DataStreamInput;
class DataStreamOutput;
class StateModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, only vertical move is allowed (new state on an existing event)
 */
namespace Scenario
{
    namespace Command
    {

        class MoveNewState final : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveNewState, "Move a new state")
            public:
            MoveNewState(
                Path<Scenario::ScenarioModel>&& scenarioPath,
                const Id<StateModel>& stateId,
                const double y);

              void undo() const override;
              void redo() const override;

              void update(
                      const Path<Scenario::ScenarioModel>&,
                      const Id<StateModel>&,
                      const double y)
              {
                  m_y = y;
              }

              const Path<Scenario::ScenarioModel>& path() const
              { return m_path; }

            protected:
              void serializeImpl(DataStreamInput&) const override;
              void deserializeImpl(DataStreamOutput&) override;

            private:
              Path<Scenario::ScenarioModel> m_path;
              Id<StateModel> m_stateId;
              double m_y{}, m_oldy{};
        };
    }
}
