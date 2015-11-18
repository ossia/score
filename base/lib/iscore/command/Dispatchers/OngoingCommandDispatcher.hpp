#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>

#include <core/command/CommandStack.hpp>

/**
 * @brief The OngoingCommandDispatcher class
 *
 * A basic, type-unsafe dispatcher for a commands
 * that have continuous edition capabilities.
 */
class OngoingCommandDispatcher final : public ICommandDispatcher
{
    public:
        OngoingCommandDispatcher(iscore::CommandStack& stack):
            ICommandDispatcher{stack}
        {

        }

        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            if(!m_cmd)
            {
                stack().disableActions();
                m_cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
                m_cmd->redo();
            }
            else
            {
                ISCORE_ASSERT(m_cmd->key() == TheCommand::static_key());
                safe_cast<TheCommand*>(m_cmd.get())->update(std::forward<Args>(args)...);
                m_cmd->redo();
            }
        }

        void commit()
        {
            if(m_cmd)
            {
                SendStrategy::Quiet::send(stack(), m_cmd.release());
                stack().enableActions();
            }
        }

        void rollback()
        {
            if(m_cmd)
            {
                m_cmd->undo();
                stack().enableActions();
            }
            m_cmd.reset();
        }

    private:
        std::unique_ptr<iscore::SerializableCommand> m_cmd;
};
