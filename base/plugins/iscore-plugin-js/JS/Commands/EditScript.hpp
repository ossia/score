#pragma once
#include <JS/Commands/JSCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

class JSProcess;
class EditScript final : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(
                JSCommandFactoryName(),
                EditScript,
                "EditScript")
    public:
        EditScript(
                Path<JSProcess>&& model, const QString& text);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        Path<JSProcess> m_model;
        QString m_old, m_new;
};
