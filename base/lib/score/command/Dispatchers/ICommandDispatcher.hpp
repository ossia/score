#pragma once
#include <score/command/Command.hpp>
namespace score
{
class CommandStackFacade;
}

/**
 * @brief The ICommandDispatcher class
 *
 * Base for command dispatchers.
 * Command dispatchers are utility class to send many commands in a specific
 * way.
 *
 * The general interface is :
 *   - submitCommand(cmd) to send a new command or update an existing one
 *   - commit() when editing is done.
 */
class ICommandDispatcher
{
public:
  ICommandDispatcher(const score::CommandStackFacade& stack) : m_stack{stack}
  {
  }

  const score::CommandStackFacade& stack() const
  {
    return m_stack;
  }

private:
  const score::CommandStackFacade& m_stack;
};
