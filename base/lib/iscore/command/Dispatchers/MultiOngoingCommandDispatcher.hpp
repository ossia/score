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
            static void rollback(const std::vector<iscore::SerializableCommand*>& cmds)
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
        MultiOngoingCommandDispatcher(iscore::CommandStackFacade& stack):
            ICommandDispatcher{stack}
        {

        }

        ~MultiOngoingCommandDispatcher()
        {
            cleanup();
        }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            stack().disableActions();
            cmd->redo();
            m_cmds.push_back(cmd);
        }

        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            if(m_cmds.empty())
            {
                stack().disableActions();
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmds.push_back(cmd);
            }
            else
            {
                iscore::SerializableCommand* last = m_cmds.back();
                if(last->key() == TheCommand::static_key())
                {
                    safe_cast<TheCommand*>(last)->update(std::forward<Args>(args)...);
                    safe_cast<TheCommand*>(last)->redo();
                }
                else
                {
                    auto cmd = new TheCommand(std::forward<Args>(args)...);
                    cmd->redo();
                    m_cmds.push_back(cmd);
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

            cleanup();
            m_cmds.clear();

            stack().enableActions();
        }

    private:
        void cleanup()
        {
            std::for_each(
                        m_cmds.rbegin(),
                        m_cmds.rend(),
                        [] (auto cmd) {
               delete cmd;
            });
        }

        std::vector<iscore::SerializableCommand*> m_cmds;
};
