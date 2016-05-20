#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class ConstraintModel;
namespace Command
{
class SetProcessUseParentDuration final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetProcessUseParentDuration, "Set process use parent duration")

    public:
        SetProcessUseParentDuration(
                Path<Process::ProcessModel>&& constraintPath,
                bool dur);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<Process::ProcessModel> m_path;
        bool m_useParentDuration{};
};
}
}
