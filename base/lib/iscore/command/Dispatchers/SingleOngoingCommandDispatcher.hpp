#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>

#include <core/command/CommandStack.hpp>

// TODO why not template the whole class on the command type ?
class SingleOngoingCommandDispatcher : public ICommandDispatcher
{
    public:
        SingleOngoingCommandDispatcher(iscore::CommandStack& stack):
            ICommandDispatcher{stack}
        {

        }

        ~SingleOngoingCommandDispatcher()
        {
            delete m_cmd;
        }

        template<typename TheCommand, typename... Args> // TODO split in two ?
        void submitCommand(Args&&... args)
        {
            if(!m_cmd)
            {
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmd = cmd;
            }
            else
            {
                Q_ASSERT(m_cmd->uid() == TheCommand::static_uid());
                static_cast<TheCommand*>(m_cmd)->update(std::forward<Args>(args)...);
                static_cast<TheCommand*>(m_cmd)->redo();
            }
        }

        void commit()
        {
            if(m_cmd)
                SendStrategy::Simple::send(stack(), m_cmd);
            m_cmd = nullptr;
        }

        void rollback()
        {
            m_cmd->undo();
            delete m_cmd;
            m_cmd = nullptr;
        }

    private:
        iscore::SerializableCommand* m_cmd{};
};
