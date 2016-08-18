#pragma once
#include <Interpolation/Commands/CommandFactory.hpp>
#include <Interpolation/Process.hpp>

#include <iscore/command/PropertyCommand.hpp>
namespace Interpolation
{
class ProcessModel;
class ChangeAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), ChangeAddress, "Change Interpolation Address")
    public:
        ChangeAddress(
                const ProcessModel& proc,
                const State::Address& addr,
                const State::Value& start,
                const State::Value& end);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<ProcessModel> m_path;
        State::Address m_oldAddr, m_newAddr;
        State::Value m_oldStart, m_newStart;
        State::Value m_oldEnd, m_newEnd;
};

// MOVEME && should apply to both Interpolation and Automation
class SetTween final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetTween, "Set curve tween")
        public:

        SetTween(Path<ProcessModel>&& path, bool newval):
            iscore::PropertyCommand{std::move(path), "tween", newval}
        {

        }
};
}
