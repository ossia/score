#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>

#include <core/command/CommandStack.hpp>

/**
 * @brief The SingleOngoingCommandDispatcher class
 *
 * Similar to OngoingCommandDispatcher but the entire class
 * is meant to be used with a single command and will fail at compile-time
 * if an incorrect command is sent.
 */
template<typename TheCommand>
class SingleOngoingCommandDispatcher : public ICommandDispatcher
{
    public:
        SingleOngoingCommandDispatcher(iscore::CommandStack& stack):
            ICommandDispatcher{stack}
        {

        }

        template<typename... Args> // TODO split in two ?
        void submitCommand(Args&&... args)
        {
            if(!m_cmd)
            {
                m_cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
                m_cmd->redo();
            }
            else
            {
                m_cmd->update(std::forward<Args>(args)...);
                m_cmd->redo();
            }
        }

        void commit()
        {
            // TODO : here we should not have "redoAndPush", just push.
            if(m_cmd)
                SendStrategy::Simple::send(stack(), m_cmd.release());
        }

        void rollback()
        {
            m_cmd->undo();
            m_cmd.reset();
        }

    private:
        std::unique_ptr<TheCommand> m_cmd;
};
