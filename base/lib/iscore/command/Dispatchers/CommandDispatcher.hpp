#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>

template<typename SendStrategy = SendStrategy::Simple>
/**
 * @brief The CommandDispatcher class
 *
 * Most basic dispatcher that will commit a command at once.
 */
class CommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        CommandDispatcher(Args&&... args):
            ICommandDispatcher{std::forward<Args&&>(args)...}
        {
        }

        void submitCommand(iscore::SerializableCommand* cmd) const
        {
            SendStrategy::send(stack(), cmd);
        }
};
