#pragma once
#include <JS/Commands/JSCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class JSProcessModel;

class EditScript final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(JSCommandFactoryName(), EditScript, "Edit a JS script")
    public:
        EditScript(
                Path<JSProcessModel>&& model,
                const QString& text);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<JSProcessModel> m_model;
        QString m_old, m_new;
};
