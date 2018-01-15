#pragma once
#include <QObject>
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
  Q_OBJECT

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

Q_SIGNALS:
  /**
   * @brief Emitted when a command was pushed on the stack
   * @param cmd the command that was pushed
   */
  void localCommand(score::Command* cmd);

  /**
   * @brief Emitted when the user calls "Undo"
   */
  void localUndo();

  /**
   * @brief Emitted when the user calls "Redo"
   */
  void localRedo();

  void localIndexChanged(int);

  void canUndoChanged(bool);
  void canRedoChanged(bool);

  void undoTextChanged(QString);
  void redoTextChanged(QString);

  void indexChanged(int);
  void stackChanged();

  // These signals are low-level and are sent on each operation that affects
  // the stacks
  void sig_undo();
  void sig_redo();
  void sig_push();
  void sig_indexChanged();

public Q_SLOTS:
  void setIndex(int index);
  void setIndexQuiet(int index);

  // These ones do not send signals
  void undoQuiet();
  void redoQuiet();

  /**
   * @brief push Pushes a command on the stack
   * @param cmd The command
   *
   * Calls cmd::redo()
   */
  void redoAndPush(score::Command* cmd);

  /**
   * @brief quietPush Pushes a command on the stack
   * @param cmd The command
   *
   * Does NOT call cmd::redo()
   */
  void push(score::Command* cmd);

  /**
   * @brief pushAndEmit Pushes a command on the stack and emit relevant signals
   * @param cmd The command
   */
  void redoAndPushQuiet(score::Command* cmd);
  void pushQuiet(score::Command* cmd);

  void undo()
  {
    undoQuiet();
    emit localUndo();
  }

  void redo()
  {
    redoQuiet();
    emit localRedo();
  }

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
      emit canUndoChanged(canUndo());

    if (pre_canRedo != canRedo())
      emit canRedoChanged(canRedo());

    if (canUndo())
      emit undoTextChanged(m_undoable.top()->description());
    else
      emit undoTextChanged("");

    if (canRedo())
      emit redoTextChanged(m_redoable.top()->description());
    else
      emit redoTextChanged("");

    emit indexChanged(m_undoable.size() - 1);
    emit stackChanged();
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
