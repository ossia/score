#pragma once
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>

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

  void submitCommand(iscore::Command* cmd) const
  {
    SendStrategy::send(stack(), cmd);
  }

  template <typename T, typename... Args>
  void submitCommand(Args&&... args) const
  {
    SendStrategy::send(stack(), new T{std::forward<Args>(args)...});
  }

  void submitCommand(std::unique_ptr<iscore::Command> cmd) const
  {
    SendStrategy::send(stack(), cmd.release());
  }
};
