#include <core/command/CommandStack.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/Todo.hpp>

#include "CommandBackupFile.hpp"
#include <iscore/command/SerializableCommand.hpp>

using namespace iscore;


CommandStackBackup::CommandStackBackup(const CommandStack& stack)
{
    // Load initial state
    for(const auto& cmd : stack.m_undoable)
    {
        savedUndo.push(CommandData{*cmd});
    }
    for(const auto& cmd : stack.m_redoable)
    {
        savedRedo.push(CommandData{*cmd});
    }
}



CommandBackupFile::CommandBackupFile(
        const iscore::CommandStack &stack,
        QObject *parent):
    QObject{parent},
    m_stack{stack},
    m_backup{m_stack}
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


    // Initial backup so that the file is always in a loadable state.
    commit();
}

QString CommandBackupFile::fileName() const
{
    return m_file.fileName();
}

void CommandBackupFile::on_push()
{
    /*
    // A new command is added to m_undoable
    // m_redoable should be cleared
    auto cmd = m_stack.m_undoable.last();
    m_backup.savedUndo.push(CommandData{*cmd});

    m_backup.savedRedo.clear();

    m_previousIndex = m_stack.currentIndex();
    */
    commit();
}

void CommandBackupFile::on_undo()
{
    /*
    // Pop from undoable to redoable
    m_backup.savedRedo.push(m_savedUndo.pop());

    m_previousIndex = m_stack.currentIndex();
    */
    commit();
}

void CommandBackupFile::on_redo()
{
    /*
    // Pop from redoable to undoable
    m_backup.savedUndo.push(m_savedRedo.pop());

    m_previousIndex = m_stack.currentIndex();
    */
    commit();
}

void CommandBackupFile::on_indexChanged()
{
    /*
    // Pop a lot
    auto index = m_stack.currentIndex();
    if(index > m_previousIndex)
    {
        for(int i = 0; i < index - m_previousIndex; i++)
        {
            m_backup.savedUndo.push(m_savedRedo.pop());
        }
    }
    else if(index < m_previousIndex)
    {
        for(int i = 0; i < m_previousIndex - index ; i++)
        {
            m_backup.savedRedo.push(m_savedUndo.pop());
        }
    }

    m_previousIndex = m_stack.currentIndex();
    */
    commit();
}

void CommandBackupFile::commit()
{
    // OPTIMIZEME: right now all the data is flushed each time.
    // It should be better to only incrementally modify the file.
    // See : http://www.boost.org/doc/libs/1_59_0/doc/html/interprocess/sharedmemorybetweenprocesses.html#interprocess.sharedmemorybetweenprocesses.mapped_file

    // Another possibility would be to save the commands to a db ?
    m_file.resize(0);
    m_file.reset();

    Serializer<DataStream> ser(&m_file);
    ser.readFrom(m_stack);

    m_file.flush();
}
