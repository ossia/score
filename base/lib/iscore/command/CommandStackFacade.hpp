#pragma once
#include <core/command/CommandStack.hpp>
namespace iscore
{
/**
 * @brief A small abstraction layer over the iscore::CommandStack
 *
 * This is a restriction of the API of the iscore::CommandStack, which allows
 * a lot of things that only make sense in the context of the base software, not plugins.
 *
 * It is meant to be used by plug-ins authors.
 */
class CommandStackFacade
{
private:
  iscore::CommandStack& m_stack;

public:
  CommandStackFacade(iscore::CommandStack& stack) : m_stack{stack}
  {
  }

  const iscore::DocumentContext& context() const
  {
    return m_stack.context();
  }

  void push(iscore::Command* cmd) const
  {
    m_stack.push(cmd);
  }

  void redoAndPush(iscore::Command* cmd) const
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
