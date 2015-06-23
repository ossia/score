#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>

template<typename SendStrategy = SendStrategy::Simple>
class CommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        CommandDispatcher(Args&&... args):
            ICommandDispatcher{std::forward<Args&&>(args)...}
        {
        }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            SendStrategy::send(stack(), cmd);
        }
};
