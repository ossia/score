#pragma once
#include <QObject>
#include <wobjectdefs.h>
#include <score/command/Command.hpp>
#include <score/command/Validity/ValidityChecker.hpp>

#include <QStack>
#include <QString>

namespace score
{
class Document;

/**
 * \class score::CommandStack
 *
 * Mostly equivalent to QUndoStack, but has added signals / slots.
 * They are used to send & receive the commands to the network, for instance.
 *
 * This class should never be used directly to send commands.
 * Instead, the various command dispatchers, in score/command/Dispatchers
 * should be used.
 */
class SCORE_LIB_BASE_EXPORT CommandStack final : public QObject
{
  W_OBJECT(CommandStack)

  friend class CommandBackupFile;
  friend struct CommandStackBackup;

public:
  explicit CommandStack(
      const score::Document& ctx, QObject* parent = nullptr);
  ~CommandStack();

  /**
   * @brief Enable the "undo" and "redo" user actions.
   *
   * Allows blocking of undo and redo.
   * Used in ongoing command classes, to prevent doing undo-redo
   * while performing an ongoing action (moving something for instance).
   */
  void enableActions();

  /**
   * @brief Disable the "undo" and "redo" user actions.
   * @see enableActions
   */
  void disableActions();

  bool canUndo() const;

  bool canRedo() const;

  //! Text shown currently in the undo action
  QString undoText() const;

  //! Text shown currently in the redo action
  QString redoText() const;

  int size() const;

  const score::Command* command(int index) const;
  int currentIndex() const;

  void markCurrentIndexAsSaved();

  bool isAtSavedIndex() const;

  QStack<score::Command*>& undoable()
  {
    return m_undoable;
  }
  QStack<score::Command*>& redoable()
  {
    return m_redoable;
  }
  const QStack<score::Command*>& undoable() const
  {
    return m_undoable;
  }
  const QStack<score::Command*>& redoable() const
  {
    return m_redoable;
  }

  const score::DocumentContext& context() const
  {
    return m_ctx;
  }

  /**
   * @brief Emitted when a command was pushed on the stack
   * @param cmd the command that was pushed
   */
  void localCommand(score::Command* cmd)
  W_SIGNAL(localCommand, cmd)

  /**
   * @brief Emitted when the user calls "Undo"
   */
  void localUndo()
  W_SIGNAL(localUndo)

  /**
   * @brief Emitted when the user calls "Redo"
   */
  void localRedo()
  W_SIGNAL(localRedo)

  void localIndexChanged(int v)
  W_SIGNAL(localIndexChanged, v)

  void canUndoChanged(bool b)
  W_SIGNAL(canUndoChanged, b)
  void canRedoChanged(bool b)
  W_SIGNAL(canRedoChanged, b)

  void undoTextChanged(QString b)
  W_SIGNAL(undoTextChanged, b)
  void redoTextChanged(QString b)
  W_SIGNAL(redoTextChanged, b)

  void indexChanged(int b)
  W_SIGNAL(indexChanged, b)
  void stackChanged()
  W_SIGNAL(stackChanged)

  // These signals are low-level and are sent on each operation that affects
  // the stacks
  void sig_undo()
  W_SIGNAL(sig_undo)
  void sig_redo()
  W_SIGNAL(sig_redo)
  void sig_push()
  W_SIGNAL(sig_push)
  void sig_indexChanged()
  W_SIGNAL(sig_indexChanged)

  void setIndex(int index);
  W_INVOKABLE(setIndex)
  void setIndexQuiet(int index);
  W_INVOKABLE(setIndexQuiet)

  // These ones do not send signals
  void undoQuiet();
  W_INVOKABLE(undoQuiet)
  void redoQuiet();
  W_INVOKABLE(redoQuiet)

  /**
   * @brief push Pushes a command on the stack
   * @param cmd The command
   *
   * Calls cmd::redo()
   */
  void redoAndPush(score::Command* cmd);
  W_INVOKABLE(redoAndPush)

  /**
   * @brief quietPush Pushes a command on the stack
   * @param cmd The command
   *
   * Does NOT call cmd::redo()
   */
  void push(score::Command* cmd);
  W_INVOKABLE(push)

  /**
   * @brief pushAndEmit Pushes a command on the stack and emit relevant signals
   * @param cmd The command
   */
  void redoAndPushQuiet(score::Command* cmd);
  W_INVOKABLE(redoAndPushQuiet)
  void pushQuiet(score::Command* cmd);
  W_INVOKABLE(pushQuiet)

  void undo()
  {
    undoQuiet();
    localUndo();
  }
  W_INVOKABLE(undo)

  void redo()
  {
    redoQuiet();
    localRedo();
  }
  W_INVOKABLE(redo)

public:
  template <typename Callable>
  /**
       * @brief updateStack Updates the undo / redo stack
       * @param c A function object of prototype void(void)
       *
       * This function takes care of keeping everything synced
       * in the GUI.
       */
  void updateStack(Callable&& c)
  {
    bool pre_canUndo{canUndo()}, pre_canRedo{canRedo()};

    m_checker();
    c();
    m_checker();

    if (pre_canUndo != canUndo())
      canUndoChanged(canUndo());

    if (pre_canRedo != canRedo())
      canRedoChanged(canRedo());

    if (canUndo())
      undoTextChanged(m_undoable.top()->description());
    else
      undoTextChanged("");

    if (canRedo())
      redoTextChanged(m_redoable.top()->description());
    else
      redoTextChanged("");

    indexChanged(m_undoable.size() - 1);
    stackChanged();
  }

  void setSavedIndex(int index);

private:
  QStack<score::Command*> m_undoable;
  QStack<score::Command*> m_redoable;

  int m_savedIndex{};

  DocumentValidator m_checker;
  const score::DocumentContext& m_ctx;
};
}
W_REGISTER_ARGTYPE(score::Command*)
