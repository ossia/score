#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>
#include <iscore/command/AggregateCommand.hpp>
#include <memory>

/**
 * @brief The MacroCommandDispatcher class
 *
 * Used to send multiple "one-shot" commands one after the other.
 * An aggregate command is required : it will put them under the same "command"
 * once in the stack.
 */

template<typename RedoStrategy_T, typename SendStrategy_T>
class GenericMacroCommandDispatcher final : public ICommandDispatcher
{
    public:
        template<typename... Args>
        GenericMacroCommandDispatcher(iscore::AggregateCommand* aggregate, Args&&... args):
                ICommandDispatcher{std::forward<Args&&>(args)...},
            m_aggregateCommand{aggregate}
        {
        }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            RedoStrategy_T::redo(*cmd);
            m_aggregateCommand->addCommand(cmd);
        }

        void commit()
        {
            if(m_aggregateCommand)
            {
                if(m_aggregateCommand->count() != 0)
                {
                    SendStrategy_T::send(stack(), m_aggregateCommand.release());
                }
                else
                {
                    m_aggregateCommand.reset();
                }
            }
        }

    protected:
        std::unique_ptr<iscore::AggregateCommand> m_aggregateCommand;
};

// Don't redo() the individual commands, and redo() the aggregate command.
using MacroCommandDispatcher = GenericMacroCommandDispatcher<RedoStrategy::Quiet, SendStrategy::Simple>;

// Don't redo anything, just push
using QuietMacroCommandDispatcher = GenericMacroCommandDispatcher<RedoStrategy::Quiet, SendStrategy::Quiet>;
