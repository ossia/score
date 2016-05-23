#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <algorithm>
#include <list>

namespace iscore
{
    /**
    * @brief AggregateCommand: allows for grouping of multiple commands.
    */
    class ISCORE_LIB_BASE_EXPORT AggregateCommand : public iscore::SerializableCommand
    {
        public:
            AggregateCommand() = default;
            virtual ~AggregateCommand();

            template<typename T>
            AggregateCommand(T* cmd) :
                AggregateCommand {}
            {
                m_cmds.push_front(cmd);
            }

            template<typename T, typename... Args>
            AggregateCommand(T* cmd, Args&& ... remaining) :
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

            const auto& commands() const
            { return m_cmds; }

        protected:
            void serializeImpl(DataStreamInput&) const override;
            void deserializeImpl(DataStreamOutput&) override;

            std::list<iscore::SerializableCommand*> m_cmds;
    };
}
