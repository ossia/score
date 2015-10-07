#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <ProcessInterface/TimeValue.hpp>

class Process;
class SetProcessDuration : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("ScenarioControl", "SetProcessDuration", "SetProcessDuration")

    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SetProcessDuration)

        SetProcessDuration(
                Path<Process>&& path,
                const TimeValue& newVal);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream& s) const override;
        void deserializeImpl(QDataStream& s) override;

    private:
        Path<Process> m_path;
        TimeValue m_old;
        TimeValue m_new;
};
