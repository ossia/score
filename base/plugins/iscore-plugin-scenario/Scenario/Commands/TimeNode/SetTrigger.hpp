#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class TimeNodeModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT SetTrigger final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetTrigger, "Change a trigger")
        public:
            SetTrigger(Path<TimeNodeModel>&& timeNodePath, State::Trigger trigger);
        ~SetTrigger();

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<TimeNodeModel> m_path;
        State::Trigger m_trigger;
        State::Trigger m_previousTrigger;
};
}
}
