#pragma once
#include <score_lib_base_export.h>

namespace score
{
class Command;
class CommandStack;
struct CommandTransaction;
struct DocumentContext;

/**
 * @brief A small abstraction layer over the score::CommandStack
 *
 * This is a restriction of the API of the score::CommandStack, which allows
 * a lot of things that only make sense in the context of the base software,
 * not plugins.
 *
 * It is meant to be used by plug-ins authors.
 */
class SCORE_LIB_BASE_EXPORT CommandStackFacade
{
private:
  score::CommandStack& m_stack;

public:
  explicit CommandStackFacade(score::CommandStack& stack) noexcept;

  const score::DocumentContext& context() const noexcept;

  void push(score::Command* cmd) const;
  void redoAndPush(score::Command* cmd) const;

  void disableActions() const;
  void enableActions() const;

  CommandTransaction transaction() const;
};
}
