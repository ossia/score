#include "CommandBackupFile.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <core/command/CommandStack.hpp>
using namespace iscore;

CommandBackupFile::CommandBackupFile(const iscore::CommandStack &stack, QObject *parent):
    QObject{parent},
    m_stack{stack}
{
    m_file.open();

    // Set-up signals
    connect(&m_stack, &CommandStack::sig_push,
            this, [&] () {
        // A new command is added to m_undoable
        // m_redoable should be cleared
        auto cmd = stack.m_undoable.last();
        m_savedUndo.push({{cmd->parentName(), cmd->name()}, cmd->serialize()});

        m_savedRedo.clear();

        m_previousIndex = m_stack.currentIndex();
        commit();
    });
    connect(&m_stack, &CommandStack::sig_undo,
            this, [&] () {
        // Pop from undoable to redoable
        m_savedRedo.push(m_savedUndo.pop());

        m_previousIndex = m_stack.currentIndex();
        commit();
    });
    connect(&m_stack, &CommandStack::sig_redo,
            this, [&] () {
        // Pop from redoable to undoable
        m_savedUndo.push(m_savedRedo.pop());

        m_previousIndex = m_stack.currentIndex();
        commit();
    });
    connect(&m_stack, &CommandStack::sig_indexChanged,
            this, [&] () {
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
    });

    // Load initial state
    for(auto&& cmd : m_stack.m_undoable)
    {
        m_savedUndo.push({{cmd->parentName(), cmd->name()}, cmd->serialize()});
    }
    for(auto&& cmd : m_stack.m_redoable)
    {
        m_savedRedo.push({{cmd->parentName(), cmd->name()}, cmd->serialize()});
    }
}

QString CommandBackupFile::fileName() const
{
    return m_file.fileName();
}

void CommandBackupFile::commit()
{
    m_file.resize(0);
    m_file.reset();

    Serializer<DataStream> ser(&m_file);
    ser.readFrom(m_stack);

    m_file.flush();
}
