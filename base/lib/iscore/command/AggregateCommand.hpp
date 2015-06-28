#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <QList>
#include <QPair>

namespace iscore
{
    /**
    * @brief AggregateCommand: allows for grouping of multiple commands.
    */
    class AggregateCommand : public iscore::SerializableCommand
    {
        public:
            AggregateCommand(QString parname, QString cmdname, QString text) :
                iscore::SerializableCommand {parname, cmdname, text}
            {
            }

            template<typename T, typename... Args>
            AggregateCommand(QString parname, QString cmdname, QString text,
                             const T& cmd, Args&& ... remaining) :
                AggregateCommand {parname, cmdname, text, std::forward<Args> (remaining)...}
            {
                m_cmds.push_front(cmd);
            }

            virtual void undo() override;
            virtual void redo() override;

            void addCommand(iscore::SerializableCommand* cmd)
            {
                m_cmds.push_back(cmd);
            }

        protected:
            virtual void serializeImpl(QDataStream&) const override;
            virtual void deserializeImpl(QDataStream&) override;

            QList<iscore::SerializableCommand*> m_cmds;
    };
}
