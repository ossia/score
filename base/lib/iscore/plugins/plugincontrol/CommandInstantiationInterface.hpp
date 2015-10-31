#pragma once
#include <QString>
#include <QByteArray>
#include <iscore/command/SerializableCommand.hpp>

namespace iscore
{
class CommandInstantiationInterface
{
    public:
        // A generic method to deserialize commands. Examples in the plug-ins.
        template<typename CommandFactory>
        iscore::SerializableCommand* instantiateUndoCommand(
                const QString& name,
                const QByteArray& data)
        {
            auto it = CommandFactory::map.find(name);
            if(it != CommandFactory::map.end())
            {
                iscore::SerializableCommand* cmd = (*(*it).second)();
                cmd->deserialize(data);
                return cmd;
            }
            else
            {
                qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read. Aborting.";
                // TODO throw an exception and stop loading the document if it isn't loadable.
                // MissingCommandException exists for this purpose.
                ISCORE_ABORT;

                return nullptr;
            }
        }

        virtual SerializableCommand* instantiateUndoCommand(
                const QString& /*name*/,
                const QByteArray& /*data*/)
        {
            return nullptr;
        }
};
}
