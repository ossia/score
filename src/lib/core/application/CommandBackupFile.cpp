// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CommandBackupFile.hpp"

#include <score/command/Command.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Bind.hpp>

#include <core/command/CommandStack.hpp>

#include <QSettings>

namespace score
{
#if defined(__EMSCRIPTEN__)
static QString makeCommandKey(const void* self)
{
  return QStringLiteral("score-backup/commands-%1").arg(quintptr(self), 0, 16);
}
#endif

CommandStackBackup::CommandStackBackup(const CommandStack& stack)
{
  // Load initial state
  savedUndo.reserve(stack.m_undoable.size());
  for(const auto& cmd : stack.m_undoable)
    savedUndo.emplace_back(*cmd);
  savedRedo.reserve(stack.m_redoable.size());
  for(const auto& cmd : stack.m_redoable)
    savedRedo.emplace_back(*cmd);
}

CommandBackupFile::CommandBackupFile(const score::CommandStack& stack, QObject* parent)
    : QObject{parent}
    , m_stack{stack}
    , m_backup{m_stack}
{
  init_connections();

#if defined(__EMSCRIPTEN__)
  m_key = makeCommandKey(this);
#else
  m_file.open();
#endif

  // Initial backup so that the file is always in a loadable state.
  commit();
}

CommandBackupFile::CommandBackupFile(
    const CommandStack& stack, const QByteArray& restored, QObject* parent)
    : QObject{parent}
    , m_stack{stack}
    , m_backup{m_stack}
{
  init_connections();

#if defined(__EMSCRIPTEN__)
  m_key = makeCommandKey(this);
  QSettings{}.setValue(m_key, restored);
#else
  m_file.open();

  m_file.resize(0);
  m_file.reset();
  m_file.write(restored);
  m_file.flush();
#endif
}

QString CommandBackupFile::fileName() const
{
#if defined(__EMSCRIPTEN__)
  return m_key;
#else
  return m_file.fileName();
#endif
}

void CommandBackupFile::init_connections()
{
  // Set-up signals
  con(m_stack, &CommandStack::sig_push, this, &CommandBackupFile::on_push);
  con(m_stack, &CommandStack::sig_undo, this, &CommandBackupFile::on_undo);
  con(m_stack, &CommandStack::sig_redo, this, &CommandBackupFile::on_redo);
  con(m_stack, &CommandStack::sig_indexChanged, this,
      &CommandBackupFile::on_indexChanged);
}

// Each command is serialized once, when pushed: undo and redo only move its
// bytes between the two stacks.
void CommandBackupFile::on_push()
{
  m_backup.savedUndo.emplace_back(*m_stack.m_undoable.top());
  m_backup.savedRedo.clear();
  commit();
}

void CommandBackupFile::on_undo()
{
  if(!m_backup.savedUndo.empty())
  {
    m_backup.savedRedo.push_back(std::move(m_backup.savedUndo.back()));
    m_backup.savedUndo.pop_back();
  }
  commit();
}

void CommandBackupFile::on_redo()
{
  if(!m_backup.savedRedo.empty())
  {
    m_backup.savedUndo.push_back(std::move(m_backup.savedRedo.back()));
    m_backup.savedRedo.pop_back();
  }
  commit();
}

void CommandBackupFile::on_indexChanged()
{
  // setIndex moves through undo and redo, which already updated the stacks.
  commit();
}

void CommandBackupFile::commit()
{
  // Changed without its signals: start over from the stack.
  if(m_backup.savedUndo.size() != std::size_t(m_stack.m_undoable.size())
     || m_backup.savedRedo.size() != std::size_t(m_stack.m_redoable.size()))
    m_backup = CommandStackBackup{m_stack};

  // The layout of DataStreamReader::read(const CommandStack&).
  auto write = [this](DataStream::Serializer& ser) {
    ser.readFrom(m_backup.savedUndo);
    ser.readFrom(m_backup.savedRedo);
    ser.insertDelimiter();
  };

#if defined(__EMSCRIPTEN__)
  QByteArray buf;
  DataStream::Serializer ser(&buf);
  write(ser);
  QSettings{}.setValue(m_key, buf);
#else
  m_file.resize(0);
  m_file.reset();

  DataStream::Serializer ser(&m_file);
  write(ser);

  m_file.flush();
#endif
}
}
