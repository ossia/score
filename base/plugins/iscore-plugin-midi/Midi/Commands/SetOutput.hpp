#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Midi
{
class ProcessModel;

class SetOutput final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(Midi::CommandFactoryName(), SetOutput, "Set Midi output")
    public:
        SetOutput(const ProcessModel& model,
                  const QString& dev);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<ProcessModel> m_model;
        QString m_old, m_new;
};

}
