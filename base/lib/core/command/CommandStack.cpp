// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QVector>
#include <QtAlgorithms>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <score/command/Command.hpp>
#include <score/command/Validity/ValidityChecker.hpp>
#include <score/document/DocumentContext.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(score::CommandStack)
namespace score
{
CommandStack::CommandStack(const score::Document& ctx, QObject* parent)
    : m_checker{score::AppComponents().interfaces<ValidityCheckerList>(), ctx}
    , m_ctx{ctx.context()}
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

bool CommandStack::canUndo() const
{
  return !m_undoable.empty();
}

bool CommandStack::canRedo() const
{
  return !m_redoable.empty();
}

QString CommandStack::undoText() const
{
  return canUndo() ? m_undoable.top()->description() : tr("Nothing to undo");
}

QString CommandStack::redoText() const
{
  return canRedo() ? m_redoable.top()->description() : tr("Nothing to redo");
}

int CommandStack::size() const
{
  return m_undoable.size() + m_redoable.size();
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

int CommandStack::currentIndex() const
{
  return m_undoable.size();
}

void CommandStack::markCurrentIndexAsSaved()
{
  setSavedIndex(currentIndex());
}

bool CommandStack::isAtSavedIndex() const
{
  return currentIndex() == m_savedIndex;
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

  sig_indexChanged();
}

void CommandStack::setIndex(int index)
{
  if (index != currentIndex())
  {
    setIndexQuiet(index);
    localIndexChanged(index);
  }
}

void CommandStack::undoQuiet()
{
  updateStack([&]() {
    auto cmd = m_undoable.pop();
    cmd->undo(m_ctx);
    m_redoable.push(cmd);

    sig_undo();
  });
}

void CommandStack::redoQuiet()
{
  updateStack([&]() {
    auto cmd = m_redoable.pop();
    cmd->redo(m_ctx);

    m_undoable.push(cmd);

    sig_redo();
  });
}

void CommandStack::redoAndPush(Command* cmd)
{
  cmd->redo(m_ctx);
  push(cmd);
}

void CommandStack::push(Command* cmd)
{
  localCommand(cmd);
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

    sig_push();
  });
}

void CommandStack::redoAndPushQuiet(Command* cmd)
{
  cmd->redo(m_ctx);
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

    sig_push();
  });
}

void CommandStack::setSavedIndex(int index)
{
  m_savedIndex = index;
}
}
