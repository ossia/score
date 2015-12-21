#pragma once
#include <JS/Commands/JSCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
namespace JS
{
class ProcessModel;

class EditScript final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(JSCommandFactoryName(), EditScript, "Edit a JS script")
    public:
        EditScript(
                Path<ProcessModel>&& model,
                const QString& text);

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
