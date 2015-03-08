#include <core/command/CommandStack.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QDebug>
using namespace iscore;

CommandStack::CommandStack(QObject* parent) :
    QObject {}
{
    this->setObjectName("CommandStack");
    this->setParent(parent);
}

const SerializableCommand* CommandStack::command(int index) const
{
    if(index < m_undoable.size())
        return m_undoable[index];
    else if((index - m_undoable.size()) < m_redoable.size())
        return m_redoable[m_redoable.size() - (index - m_undoable.size()) - 1];
    return nullptr;
}


void CommandStack::setIndex(int index)
{
    while(index >= 0 && currentIndex() != index)
    {
        if(index < currentIndex())
            undo();
        else
            redo();
    }
}

void CommandStack::undo()
{
    updateStack([&] ()
    {
        auto cmd = m_undoable.pop();
        cmd->undo();
        m_redoable.push(cmd);
    });
}

void CommandStack::redo()
{
    updateStack([&] ()
    {
        auto cmd = m_redoable.pop();
        cmd->redo();

        m_undoable.push(cmd);
    });
}

void CommandStack::push(SerializableCommand* cmd)
{
    cmd->redo();
    quietPush(cmd);
}

void CommandStack::quietPush(SerializableCommand* cmd)
{
    updateStack([&] ()
    {
        m_undoable.push(cmd);

        qDeleteAll(m_redoable);
        m_redoable.clear();
    });
}

void CommandStack::pushAndEmit(SerializableCommand* cmd)
{
    emit push_start(cmd);
    push(cmd);
}
