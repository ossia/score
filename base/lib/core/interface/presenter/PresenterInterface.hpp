#pragma once
#include <QString>

namespace iscore
{
    class SerializableCommand;

    namespace IPresenter
    {
        /**
         * @brief instantiateUndoCommand Is used to generate a Command from its serialized data.
         * @param parent_name The name of the object able to generate the command. Must be a CustomCommand.
         * @param name The name of the command to generate.
         * @param data The data of the command.
         *
         * Ownership of the command is transferred to the caller, and he must delete it.
         */
        iscore::SerializableCommand*
        instantiateUndoCommand (const QString& parent_name,
                                const QString& name,
                                const QByteArray& data);
    }
}
