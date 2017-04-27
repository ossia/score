#include <QVector>
#include <QtAlgorithms>
#include <core/command/CommandStack.hpp>

#include <core/document/Document.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/command/Validity/ValidityChecker.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace iscore
{
CommandStack::CommandStack(const iscore::Document& ctx, QObject* parent)
    : m_checker{iscore::AppComponents().interfaces<ValidityCheckerList>(),
                ctx}
{
  this->setObjectName("CommandStack");
  this->setParent(parent);
}

CommandStack::~CommandStack()
{
  qDeleteAll(m_undoable);
  qDeleteAll(m_redoable);
}

void CommandStack::enableActions()
{
  canUndoChanged(canUndo());
  canRedoChanged(canRedo());
}

void CommandStack::disableActions()
{
  canUndoChanged(false);
  canRedoChanged(false);
}

const Command* CommandStack::command(int index) const
{
  if (index < m_undoable.size())
  {
    return m_undoable[index];
  }
  else if ((index - m_undoable.size()) < m_redoable.size())
  {
    return m_redoable[m_redoable.size() - (index - m_undoable.size()) - 1];
  }
  else
  {
    return nullptr;
  }
}

void CommandStack::setIndexQuiet(int index)
{
  while (index >= 0 && currentIndex() != index)
  {
    if (index < currentIndex())
      undoQuiet();
    else
      redoQuiet();
  }

  emit sig_indexChanged();
}

void CommandStack::setIndex(int index)
{
  if (index != currentIndex())
  {
    setIndexQuiet(index);
    emit localIndexChanged(index);
  }
}

void CommandStack::undoQuiet()
{
  updateStack([&]() {
    auto cmd = m_undoable.pop();
    cmd->undo();
    m_redoable.push(cmd);

    emit sig_undo();
  });
}

void CommandStack::redoQuiet()
{
  updateStack([&]() {
    auto cmd = m_redoable.pop();
    cmd->redo();

    m_undoable.push(cmd);

    emit sig_redo();
  });
}

void CommandStack::redoAndPush(Command* cmd)
{
  cmd->redo();
  push(cmd);
}

void CommandStack::push(Command* cmd)
{
  emit localCommand(cmd);
  updateStack([&]() {
    // We lose the state we saved
    if (currentIndex() < m_savedIndex)
      setSavedIndex(-1);

    // Push operation
    m_undoable.push(cmd);

    if (!m_redoable.empty())
    {
      qDeleteAll(m_redoable);
      m_redoable.clear();
    }

    emit sig_push();
  });
}

void CommandStack::redoAndPushQuiet(Command* cmd)
{
  cmd->redo();
  pushQuiet(cmd);
}

void CommandStack::pushQuiet(Command* cmd)
{
  updateStack([&]() {
    // We lose the state we saved
    if (currentIndex() < m_savedIndex)
      setSavedIndex(-1);

    // Push operation
    m_undoable.push(cmd);

    if (!m_redoable.empty())
    {
      qDeleteAll(m_redoable);
      m_redoable.clear();
    }

    emit sig_push();
  });
}

void CommandStack::setSavedIndex(int index)
{
  m_savedIndex = index;
}
}
