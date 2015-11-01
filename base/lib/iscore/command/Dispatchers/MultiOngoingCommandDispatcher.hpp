#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>
#include <core/command/CommandStack.hpp>

// Creates commands on a list and keep updating the latest command
// up to the next new command.
namespace RollbackStrategy
{
    struct Simple
    {
            static void rollback(const QList<iscore::SerializableCommand*>& cmds)
            {
                for(int i = cmds.size() - 1; i >= 0; --i)
                {
                    cmds[i]->undo();
                }
            }
    };
}

/**
 * @brief The MultiOngoingCommandDispatcher class
 *
 * Used for complex real-time editing.
 * This dispatcher has an array of commands :
 * as long as the commands sent through submitCommand
 * are mergeable, they will be merged with the latest command on the array.
 *
 * When a new command is encoutered, it is put in a new place in the array.
 */
class MultiOngoingCommandDispatcher final : public ICommandDispatcher
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
            stack().disableActions();
            cmd->redo();
            m_cmds.append(cmd);
        }

        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            if(m_cmds.empty())
            {
                stack().disableActions();
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmds.append(cmd);
            }
            else
            {
                iscore::SerializableCommand* last = m_cmds.last();
                if(last->uid() == TheCommand::static_uid())
                {
                    safe_cast<TheCommand*>(last)->update(std::forward<Args>(args)...);
                    safe_cast<TheCommand*>(last)->redo();
                }
                else
                {
                    auto cmd = new TheCommand(std::forward<Args>(args)...);
                    cmd->redo();
                    m_cmds.append(cmd);
                }
            }
        }

        // Give it something that behaves like AggregateCommand
        template<typename CommitCommand>
        void commit()
        {
            if(!m_cmds.empty())
            {
                auto theCmd = new CommitCommand;
                for(auto& cmd : m_cmds)
                {
                    theCmd->addCommand(cmd);
                }

                SendStrategy::Quiet::send(stack(), theCmd);
                m_cmds.clear();

                stack().enableActions();
            }
        }

        template<typename RollbackStrategy>
        void rollback()
        {
            RollbackStrategy::rollback(m_cmds);

            qDeleteAll(m_cmds);
            m_cmds.clear();

            stack().enableActions();
        }

    private:
        QList<iscore::SerializableCommand*> m_cmds;
};
