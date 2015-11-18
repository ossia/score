#include "CommandBackupFile.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <core/command/CommandStack.hpp>
#include <iscore/tools/Todo.hpp>

using namespace iscore;
CommandBackupFile::CommandBackupFile(const iscore::CommandStack &stack, QObject *parent):
    QObject{parent},
    m_stack{stack}
{
    m_file.open();

    // Set-up signals
    con(m_stack, &CommandStack::sig_push,
            this, &CommandBackupFile::on_push);
    con(m_stack, &CommandStack::sig_undo,
            this, &CommandBackupFile::on_undo);
    con(m_stack, &CommandStack::sig_redo,
            this, &CommandBackupFile::on_redo);
    con(m_stack, &CommandStack::sig_indexChanged,
            this, &CommandBackupFile::on_indexChanged);

    // Load initial state
    for(const auto& cmd : m_stack.m_undoable)
    {
        m_savedUndo.push({{cmd->parentKey(), cmd->key()}, cmd->serialize()});
    }
    for(const auto& cmd : m_stack.m_redoable)
    {
        m_savedRedo.push({{cmd->parentKey(), cmd->key()}, cmd->serialize()});
    }

    // Initial backup so that the file is always in a loadable state.
    commit();
}

QString CommandBackupFile::fileName() const
{
    return m_file.fileName();
}

void CommandBackupFile::on_push()
{
    // A new command is added to m_undoable
    // m_redoable should be cleared
    auto cmd = m_stack.m_undoable.last();
    m_savedUndo.push({{cmd->parentKey(), cmd->key()}, cmd->serialize()});

    m_savedRedo.clear();

    m_previousIndex = m_stack.currentIndex();
    commit();
}

void CommandBackupFile::on_undo()
{
    // Pop from undoable to redoable
    m_savedRedo.push(m_savedUndo.pop());

    m_previousIndex = m_stack.currentIndex();
    commit();
}

void CommandBackupFile::on_redo()
{
    // Pop from redoable to undoable
    m_savedUndo.push(m_savedRedo.pop());

    m_previousIndex = m_stack.currentIndex();
    commit();
}

void CommandBackupFile::on_indexChanged()
{
    // Pop a lot
    auto index = m_stack.currentIndex();
    if(index > m_previousIndex)
    {
        for(int i = 0; i < index - m_previousIndex; i++)
        {
            m_savedUndo.push(m_savedRedo.pop());
        }
    }
    else if(index < m_previousIndex)
    {
        for(int i = 0; i < m_previousIndex - index ; i++)
        {
            m_savedRedo.push(m_savedUndo.pop());
        }
    }

    m_previousIndex = m_stack.currentIndex();
    commit();
}

void CommandBackupFile::commit()
{
    // OPTIMIZEME: right now all the data is flushed each time.
    // It should be better to only incrementally modify the file.

    // Another possibility would be to save the commands to a db ?
    m_file.resize(0);
    m_file.reset();

    Serializer<DataStream> ser(&m_file);
    ser.readFrom(m_stack);

    m_file.flush();
}
