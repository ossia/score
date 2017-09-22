#pragma once
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Dispatchers/SendStrategy.hpp>

template <typename SendStrategy = SendStrategy::Simple>
/**
 * @brief The CommandDispatcher class
 *
 * Most basic dispatcher that will commit a command at once.
 */
class CommandDispatcher final : public ICommandDispatcher
{
public:
  template <typename... Args>
  CommandDispatcher(Args&&... args)
      : ICommandDispatcher{std::forward<Args&&>(args)...}
  {
  }

  void submitCommand(score::Command* cmd) const
  {
    SendStrategy::send(stack(), cmd);
  }

  template <typename T, typename... Args>
  void submitCommand(Args&&... args) const
  {
    SendStrategy::send(stack(), new T{std::forward<Args>(args)...});
  }

  void submitCommand(std::unique_ptr<score::Command> cmd) const
  {
    SendStrategy::send(stack(), cmd.release());
  }
};
