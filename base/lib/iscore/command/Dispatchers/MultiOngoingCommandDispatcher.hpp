#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>
#include <core/command/CommandStack.hpp>

// Creates commands on a list and keep updating the latest command
// up to the next new command.
class MultiOngoingCommandDispatcher : public ICommandDispatcher
{
    public:
        MultiOngoingCommandDispatcher(iscore::CommandStack& stack):
            ICommandDispatcher{stack}
        {

        }

        ~MultiOngoingCommandDispatcher()
        {
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                delete m_cmds[i];
            }
         }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            cmd->redo();
            m_cmds.append(cmd);
        }

        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            if(m_cmds.empty())
            {
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmds.append(cmd);
            }
            else
            {
                iscore::SerializableCommand* last = m_cmds.last();
                if(last->uid() == TheCommand::static_uid())
                {
                    static_cast<TheCommand*>(last)->update(std::forward<Args>(args)...);
                    static_cast<TheCommand*>(last)->redo();
                }
                else
                {
                    auto cmd = new TheCommand(std::forward<Args>(args)...);
                    cmd->redo();
                    m_cmds.append(cmd);
                }
            }
        }

        template<typename CommitCommand>
        void commit()
        {
            auto theCmd = new CommitCommand;
            for(auto& cmd : m_cmds)
            {
                theCmd->addCommand(cmd);
            }

            SendStrategy::Quiet::send(stack(), theCmd);
            m_cmds.clear();
        }

        void rollback()
        {
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                m_cmds[i]->undo();
                delete m_cmds[i];
            }

            m_cmds.clear();
        }

    private:
        QList<iscore::SerializableCommand*> m_cmds;
};
