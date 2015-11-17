#pragma once
#include <Loop/Commands/LoopCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

class LoopProcessModel;
class EditScript final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(
                LoopCommandFactoryName(),
                EditScript,
                "EditScript")
    public:
        EditScript(
                Path<LoopProcessModel>&& model,
                const QString& text);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        Path<LoopProcessModel> m_model;
        QString m_old, m_new;
};
