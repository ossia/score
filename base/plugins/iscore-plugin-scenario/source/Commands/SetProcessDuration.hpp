#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <Commands/ScenarioCommandFactory.hpp>

class Process;
class SetProcessDuration : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), SetProcessDuration, "SetProcessDuration")

    public:

        SetProcessDuration(
                Path<Process>&& path,
                const TimeValue& newVal);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream& s) const override;
        void deserializeImpl(QDataStream& s) override;

    private:
        Path<Process> m_path;
        TimeValue m_old;
        TimeValue m_new;
};
