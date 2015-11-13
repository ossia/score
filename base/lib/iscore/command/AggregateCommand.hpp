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
            AggregateCommand() = default;
            virtual ~AggregateCommand();

            template<typename T, typename... Args>
            AggregateCommand(const T& cmd, Args&& ... remaining) :
                AggregateCommand {std::forward<Args> (remaining)...}
            {
                m_cmds.push_front(cmd);
            }

            void undo() const override;
            void redo() const override;

            void addCommand(iscore::SerializableCommand* cmd)
            {
                m_cmds.push_back(cmd);
            }

            int count() const
            { return m_cmds.size(); }

        protected:
            void serializeImpl(QDataStream&) const override;
            void deserializeImpl(QDataStream&) override;

            QList<iscore::SerializableCommand*> m_cmds;
    };
}
