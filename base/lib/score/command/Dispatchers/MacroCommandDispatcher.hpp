#pragma once
#include <memory>
#include <score/command/AggregateCommand.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Dispatchers/SendStrategy.hpp>

/**
 * @brief The MacroCommandDispatcher class
 *
 * Used to send multiple "one-shot" commands one after the other.
 * An aggregate command is required : it will put them under the same "command"
 * once in the stack.
 */

template <typename Command_T, typename RedoStrategy_T, typename SendStrategy_T>
class GenericMacroCommandDispatcher final : public ICommandDispatcher
{
public:
  template <typename... Args>
  GenericMacroCommandDispatcher(Args&&... args)
      : ICommandDispatcher{std::forward<Args&&>(args)...}
      , m_aggregateCommand{std::make_unique<Command_T>()}
  {
    static_assert(
        std::is_base_of<score::AggregateCommand, Command_T>::value,
        "MacroCommandDispatcher: Command_T must be AggregateCommand-derived");
  }

  template <typename... Args>
  GenericMacroCommandDispatcher(std::unique_ptr<score::AggregateCommand> cmd, Args&&... args)
      : ICommandDispatcher{std::forward<Args&&>(args)...}
      , m_aggregateCommand{std::move(cmd)}
  {
    SCORE_ASSERT(m_aggregateCommand);
  }

  void submitCommand(score::Command* cmd)
  {
    RedoStrategy_T::redo(stack().context(), *cmd);
    m_aggregateCommand->addCommand(cmd);
  }

  void commit()
  {
    if (m_aggregateCommand)
    {
      if (m_aggregateCommand->count() != 0)
      {
        SendStrategy_T::send(stack(), m_aggregateCommand.release());
      }

      m_aggregateCommand.reset();
    }
  }

  void rollback()
  {
    if (m_aggregateCommand)
    {
      m_aggregateCommand->undo(stack().context());
      m_aggregateCommand.reset();
    }
  }

  auto command() const
  {
    return m_aggregateCommand.get();
  }

protected:
  std::unique_ptr<Command_T> m_aggregateCommand;
};

// Don't redo the individual commands, and redo() the aggregate command.
template <typename Command_T>
using MacroCommandDispatcher = GenericMacroCommandDispatcher<
    Command_T,
    RedoStrategy::Quiet,
    SendStrategy::Simple>;

// Redo the individual commands, don't redo the aggregate command
template <typename Command_T>
using RedoMacroCommandDispatcher = GenericMacroCommandDispatcher<
    Command_T,
    RedoStrategy::Redo,
    SendStrategy::Quiet>;

// Don't redo anything, just push
template <typename Command_T>
using QuietMacroCommandDispatcher = GenericMacroCommandDispatcher<
    Command_T,
    RedoStrategy::Quiet,
    SendStrategy::Quiet>;
