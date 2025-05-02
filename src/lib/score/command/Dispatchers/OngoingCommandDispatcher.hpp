#pragma once
#include <score/command/CommandStackFacade.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Dispatchers/SendStrategy.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/SafeCast.hpp>

#include <memory>

/**
 * @brief The OngoingCommandDispatcher class
 *
 * A basic, type-unsafe dispatcher for a commands
 * that have continuous edition capabilities.
 *
 * That is, it is useful when you want to have a command that has a
 * long initialization but a very fast update. For instance, moving an object :
 * initializing the command is (relatively) long so we don't want to create a
 * new one at every mouse movement. <br> Instead, such commands have an
 * `update()` function with the same arguments than the used constructor. <br>
 * This dispatcher will call the correct method of the given command whether
 * we're initializing it for the first time, or modifying the existing command.
 *
 *
 */
class OngoingCommandDispatcher final : public ICommandDispatcher
{
public:
  OngoingCommandDispatcher(const score::CommandStackFacade& stack)
      : ICommandDispatcher{stack}
  {
  }

  //! Call this repeatedly to make the command, for instance on click and when
  //! the mouse moves.
  template <typename TheCommand, typename... Args>
  void submit(Args&&... args)
  {
    if(!m_cmd)
    {
      stack().disableActions();
      m_cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
      m_cmd->redo(stack().context());
    }
    else
    {
      if(m_cmd->key() != TheCommand::static_key())
      {
        throw std::runtime_error(
            "Ongoing command mismatch: current command " + m_cmd->key().toString()
            + " does not match new command " + TheCommand{}.key().toString());
      }

      safe_cast<TheCommand*>(m_cmd.get())->update(std::forward<Args>(args)...);
      m_cmd->redo(stack().context());
    }
  }

  //! When the command is finished and can be sent to the undo - redo stack.
  //! For instance on mouse release.
  void commit()
  {
    if(m_cmd)
    {
      SendStrategy::Quiet::send(stack(), m_cmd.release());
      stack().enableActions();
    }
  }

  //! If the command has to be reverted, for instance when pressing escape.
  void rollback()
  {
    if(m_cmd)
    {
      m_cmd->undo(stack().context());
      stack().enableActions();
    }
    m_cmd.reset();
  }

private:
  std::unique_ptr<score::Command> m_cmd;
};
