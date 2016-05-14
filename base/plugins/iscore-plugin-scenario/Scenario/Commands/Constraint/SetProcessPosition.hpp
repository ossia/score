#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class ConstraintModel;
namespace Command
{
class SetProcessPosition final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetProcessPosition, "Set process position")

    public:
        SetProcessPosition(
                Path<Scenario::ConstraintModel>&& cst,
                const Id<Process::ProcessModel>& proc,
                const Id<Process::ProcessModel>& proc2);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<Scenario::ConstraintModel> m_path;
        Id<Process::ProcessModel> m_proc, m_proc2;
};

class SwapProcessPosition final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapProcessPosition, "Set process position")

    public:
        SwapProcessPosition(
                Path<Scenario::ConstraintModel>&& cst,
                const Id<Process::ProcessModel>& proc,
                const Id<Process::ProcessModel>& proc2);

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
