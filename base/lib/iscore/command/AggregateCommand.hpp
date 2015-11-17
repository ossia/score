#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <vector>
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

            template<typename T>
            AggregateCommand(T* cmd) :
                AggregateCommand {}
            {
                m_cmds.push_back(cmd);
                std::reverse(m_cmds.begin(), m_cmds.end());
            }

            template<typename T, typename... Args>
            AggregateCommand(T* cmd, Args&& ... remaining) :
                AggregateCommand {std::forward<Args> (remaining)...}
            {
                m_cmds.push_back(cmd);
            }

            void undo() const override;
            void redo() const override;

            void addCommand(iscore::SerializableCommand* cmd)
            {
                m_cmds.push_back(cmd);
            }

            int count() const
            { return m_cmds.size(); }

            const auto& commands() const
            { return m_cmds; }

        protected:
            void serializeImpl(QDataStream&) const override;
            void deserializeImpl(QDataStream&) override;

            std::vector<iscore::SerializableCommand*> m_cmds;
    };
}
