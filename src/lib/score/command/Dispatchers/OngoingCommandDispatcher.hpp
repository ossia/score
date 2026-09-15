#pragma once
#include <score/command/CommandStackFacade.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Dispatchers/SendStrategy.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/SafeCast.hpp>

#include <QObject>

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
class SCORE_LIB_BASE_EXPORT OngoingCommandDispatcher final : public ICommandDispatcher
{
public:
  explicit OngoingCommandDispatcher(const score::CommandStackFacade& stack);
  ~OngoingCommandDispatcher();

  // In Document.cpp
  void watch(const QObject* obj);
  void unwatch(const QObject* obj);

  //! Whether obj is the object of the current -- or, if none is in progress,
  //! the most recent -- interaction. Kept past commit() so that re-editing the
  //! same object does not have to re-arm its watch.
  bool watching(const QObject* obj) const noexcept { return m_watched == obj; }

  //! Call this repeatedly to make the command, for instance on click and when
  //! the mouse moves.
  template <typename TheCommand, typename Watched, typename... Args>
  void submit(Watched&& watched, Args&&... args)
  {
    using decayed = std::remove_cvref_t<Watched>;
    const QObject* target{};
    if constexpr(std::is_pointer_v<decayed>)
    {
      static_assert(std::is_base_of_v<QObject, std::remove_cvref_t<decltype(*watched)>>);
      target = watched;
    }
    else
    {
      static_assert(std::is_base_of_v<QObject, decayed>);
      target = &watched;
    }

    // An interaction that never got its end -- a drag whose mouse grab the
    // scene took away without delivering the release, a widget hidden
    // mid-edit -- leaves its command sitting here. update() only refreshes a
    // command's parameters and not what it points at, so carrying that one
    // over would send this interaction's edits to the *previous* object: the
    // widget under the mouse would move while the value went somewhere else.
    // The stale command has already been redone, so close it properly.
    if(m_cmd && (m_watched != target || m_cmd->key() != TheCommand::static_key()))
    {
      // A different command type means the two interactions are unrelated, so
      // whoever owned the first one never finished it. That used to throw;
      // closing it is friendlier, but it is still worth saying out loud.
      if(m_cmd->key() != TheCommand::static_key())
      {
        qDebug() << "OngoingCommandDispatcher: closing unfinished"
                 << m_cmd->key().toString().c_str() << "to start"
                 << TheCommand::static_key().toString().c_str();
      }
      commit();
    }

    watch(target);

    if(!m_cmd)
    {
      stack().disableActions();
      m_cmd = std::make_unique<TheCommand>(watched, std::forward<Args>(args)...);
      m_cmd->redo(stack().context());
    }
    else
    {
      safe_cast<TheCommand*>(m_cmd.get())->update(watched, std::forward<Args>(args)...);
      m_cmd->redo(stack().context());
    }
  }

  //! When the command is finished and can be sent to the undo - redo stack.
  //! For instance on mouse release.
  void commit();

  //! If the command has to be reverted, for instance when pressing escape.
  void rollback();
  void rollback_without_undo();

private:
  std::unique_ptr<score::Command> m_cmd;
  const QObject* m_watched{};
};
