#include <core/command/CommandStack.hpp>

using namespace iscore;

CommandStack::CommandStack(QObject* parent) :
    QObject {}
{
    this->setObjectName("CommandStack");
    this->setParent(parent);
}

CommandStack::~CommandStack()
{
    for(auto& elt : m_undoable)
        delete elt;
    for(auto& elt : m_redoable)
        delete elt;

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
            undoQuiet();
        else
            redoQuiet();
    }
}

void CommandStack::undoQuiet()
{
    updateStack([&] ()
    {
        auto cmd = m_undoable.pop();
        cmd->undo();
        m_redoable.push(cmd);
    });
}

void CommandStack::redoQuiet()
{
    updateStack([&] ()
    {
        auto cmd = m_redoable.pop();
        cmd->redo();

        m_undoable.push(cmd);
    });
}

void CommandStack::redoAndPush(SerializableCommand* cmd)
{
    cmd->redo();
    push(cmd);
}

void CommandStack::push(SerializableCommand* cmd)
{
    emit localCommand(cmd);
    updateStack([&] ()
    {
        // We lose the state we saved
        if(currentIndex() < m_savedIndex)
            setSavedIndex(-1);

        // Push operation
        m_undoable.push(cmd);

        if(!m_redoable.empty())
        {
            qDeleteAll(m_redoable);
            m_redoable.clear();
        }
    });
}

void CommandStack::redoAndPushQuiet(SerializableCommand* cmd)
{
    cmd->redo();
    pushQuiet(cmd);
}

void CommandStack::pushQuiet(SerializableCommand* cmd)
{
    updateStack([&] ()
    {
        // We lose the state we saved
        if(currentIndex() < m_savedIndex)
            setSavedIndex(-1);

        // Push operation
        m_undoable.push(cmd);

        if(!m_redoable.empty())
        {
            qDeleteAll(m_redoable);
            m_redoable.clear();
        }
    });
}

void CommandStack::setSavedIndex(int index)
{
    m_savedIndex = index;
}
