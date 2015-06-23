#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>
#include <iscore/command/AggregateCommand.hpp>

class MacroCommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        MacroCommandDispatcher(iscore::AggregateCommand* aggregate, Args&&... args):
                ICommandDispatcher{std::forward<Args&&>(args)...},
            m_aggregateCommand{aggregate}
        {
        }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            m_aggregateCommand->addCommand(cmd);
        }

        void commit()
        {
            if(m_aggregateCommand)
            {
                SendStrategy::Simple::send(stack(), m_aggregateCommand);
                m_aggregateCommand = nullptr;
            }
        }

    protected:
        iscore::AggregateCommand* m_aggregateCommand {};
};
