#pragma once
#include <core/command/CommandStack.hpp>
namespace score
{
/**
 * @brief A small abstraction layer over the score::CommandStack
 *
 * This is a restriction of the API of the score::CommandStack, which allows
 * a lot of things that only make sense in the context of the base software,
 * not plugins.
 *
 * It is meant to be used by plug-ins authors.
 */
class CommandStackFacade
{
private:
  score::CommandStack& m_stack;

public:
  CommandStackFacade(score::CommandStack& stack) : m_stack{stack}
  {
  }

  const score::DocumentContext& context() const
  {
    return m_stack.context();
  }

  void push(score::Command* cmd) const
  {
    m_stack.push(cmd);
  }

  void redoAndPush(score::Command* cmd) const
  {
    m_stack.redoAndPush(cmd);
  }

  void disableActions() const
  {
    m_stack.disableActions();
  }

  void enableActions() const
  {
    m_stack.enableActions();
  }
};
}
