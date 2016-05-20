#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process { class ProcessModel; }

namespace Scenario
{
namespace Command
{
class SetProcessDuration final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetProcessDuration, "Change a process duration")

    public:

        SetProcessDuration(
                Path<Process::ProcessModel>&& path,
                TimeValue newVal);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<Process::ProcessModel> m_path;
        TimeValue m_old;
        TimeValue m_new;
};

}
}
