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
class PutProcessBefore final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), PutProcessBefore, "Set process position")

    public:
        // Put proc2 before proc
        PutProcessBefore(
                Path<Scenario::ConstraintModel>&& cst,
                Id<Process::ProcessModel> proc,
                Id<Process::ProcessModel> proc2);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<Scenario::ConstraintModel> m_path;
        Id<Process::ProcessModel> m_proc, m_proc2;
};


class PutProcessToEnd final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), PutProcessToEnd, "Set process position")

    public:
        // Put proc2 before proc
        PutProcessToEnd(
                Path<Scenario::ConstraintModel>&& cst,
                Id<Process::ProcessModel> proc);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<Scenario::ConstraintModel> m_path;
        Id<Process::ProcessModel> m_proc, m_proc_after;
};

class SwapProcessPosition final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapProcessPosition, "Set process position")

    public:
        SwapProcessPosition(
                Path<Scenario::ConstraintModel>&& cst,
                Id<Process::ProcessModel> proc,
                Id<Process::ProcessModel> proc2);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<Scenario::ConstraintModel> m_path;
        Id<Process::ProcessModel> m_proc, m_proc2;
};
}
}
