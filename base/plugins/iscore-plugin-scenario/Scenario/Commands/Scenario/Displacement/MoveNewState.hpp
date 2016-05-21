#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, only vertical move is allowed (new state on an existing event)
 */
namespace Scenario
{
class StateModel;
class ScenarioModel;

namespace Command
{

class MoveNewState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveNewState, "Move a new state")
        public:
            MoveNewState(
                Path<Scenario::ScenarioModel>&& scenarioPath,
                Id<StateModel> stateId,
                double y);

        void undo() const override;
        void redo() const override;

        void update(
                const Path<Scenario::ScenarioModel>&,
                const Id<StateModel>&,
                double y)
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
