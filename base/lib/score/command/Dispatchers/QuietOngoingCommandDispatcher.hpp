#pragma once
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Dispatchers/SendStrategy.hpp>

#include <core/command/CommandStack.hpp>

namespace score
{
class QuietOngoingCommandDispatcher final : public ICommandDispatcher
{
public:
  QuietOngoingCommandDispatcher(const score::CommandStackFacade& stack)
      : ICommandDispatcher{stack}
  {
  }

  template <typename TheCommand, typename... Args>
  void submitCommand(Args&&... args)
  {
    if (!m_cmd)
    {
      stack().disableActions();
      m_cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
    }
    else
    {
      SCORE_ASSERT(m_cmd->key() == TheCommand::static_key());
      safe_cast<TheCommand*>(m_cmd.get())->update(std::forward<Args>(args)...);
    }
  }

  template<typename Strategy = SendStrategy::Simple> // TODO why ?
  void commit()
  {
    if (m_cmd)
    {
      Strategy::send(stack(), m_cmd.release());
      stack().enableActions();
    }
  }

  void rollback()
  {
    if (m_cmd)
    {
      stack().enableActions();
    }
    m_cmd.reset();
  }

private:
  std::unique_ptr<score::Command> m_cmd;
};
}
