#pragma once
#include <score/command/Command.hpp>
#include <score/command/Validity/ValidityChecker.hpp>

#include <QObject>
#include <QStack>
#include <QString>

#include <verdigris>

namespace score
{
class Document;
struct CommandTransaction;
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
  explicit CommandStack(const score::Document& ctx, QObject* parent = nullptr);
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

  QStack<score::Command*>& undoable() { return m_undoable; }
  QStack<score::Command*>& redoable() { return m_redoable; }
  const QStack<score::Command*>& undoable() const { return m_undoable; }
  const QStack<score::Command*>& redoable() const { return m_redoable; }

  const score::DocumentContext& context() const { return m_ctx; }

  /**
   * @brief Emitted when a command was pushed on the stack
   * @param cmd the command that was pushed
   */
  void localCommand(score::Command* cmd)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, localCommand, cmd)

  /**
   * @brief Emitted when the user calls "Undo"
   */
  void localUndo() E_SIGNAL(SCORE_LIB_BASE_EXPORT, localUndo)

  /**
   * @brief Emitted when the user calls "Redo"
   */
  void localRedo() E_SIGNAL(SCORE_LIB_BASE_EXPORT, localRedo)

  void localIndexChanged(int v) E_SIGNAL(SCORE_LIB_BASE_EXPORT, localIndexChanged, v)

  void canUndoChanged(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, canUndoChanged, b)
  void canRedoChanged(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, canRedoChanged, b)

  void undoTextChanged(QString b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, undoTextChanged, b)
  void redoTextChanged(QString b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, redoTextChanged, b)

  void indexChanged(int b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, indexChanged, b)

  void stackChanged() E_SIGNAL(SCORE_LIB_BASE_EXPORT, stackChanged)

  void saveIndexChanged(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, saveIndexChanged, b)

  // These signals are low-level and are sent on each operation that
  // affects the stacks
  void sig_undo() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sig_undo)
  void sig_redo() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sig_redo)
  void sig_push() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sig_push)
  void sig_indexChanged() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sig_indexChanged)

  void setIndex(int index);
  W_INVOKABLE(setIndex)
  void setIndexQuiet(int index);
  W_INVOKABLE(setIndexQuiet)

  // These ones do not send signals
  void undoQuiet();
  W_INVOKABLE(undoQuiet)
  void redoQuiet();
  W_INVOKABLE(redoQuiet)

  void beginTransaction() E_SIGNAL(SCORE_LIB_BASE_EXPORT, beginTransaction)
  void endTransaction() E_SIGNAL(SCORE_LIB_BASE_EXPORT, endTransaction)

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

    if(pre_canUndo != canUndo())
      canUndoChanged(canUndo());

    if(pre_canRedo != canRedo())
      canRedoChanged(canRedo());

    if(canUndo())
      undoTextChanged(m_undoable.top()->description());
    else
      undoTextChanged("");

    if(canRedo())
      redoTextChanged(m_redoable.top()->description());
    else
      redoTextChanged("");

    indexChanged(m_undoable.size() - 1);
    stackChanged();
  }

  void setSavedIndex(int index);

  void validateDocument() const;

  CommandTransaction transaction();

private:
  friend struct CommandTransaction;
  QStack<score::Command*> m_undoable;
  QStack<score::Command*> m_redoable;

  int m_savedIndex{};
  int m_inTransaction{};

  DocumentValidator m_checker;
  const score::DocumentContext& m_ctx;
};

struct CommandTransaction
{
  CommandStack& self;

  explicit CommandTransaction(CommandStack& self)
      : self{self}
  {
    if(self.m_inTransaction == 0)
    {
      self.beginTransaction();
    }
    self.m_inTransaction++;
  }
  CommandTransaction(const CommandTransaction& other) = delete;
  CommandTransaction(CommandTransaction&& other) = delete;
  CommandTransaction& operator=(const CommandTransaction& other) = delete;
  CommandTransaction& operator=(CommandTransaction&& other) = delete;

  ~CommandTransaction()
  {
    SCORE_ASSERT(self.m_inTransaction > 0);

    self.m_inTransaction--;
    if(self.m_inTransaction == 0)
    {
      self.endTransaction();
    }
  }
};
}
W_REGISTER_ARGTYPE(score::Command*)
