#include "OngoingCommandManager.hpp"
#include <QDebug>
void OngoingCommandManager::send(iscore::SerializableCommand* cmd)
{
    cmd->enableMerging();
    if(m_ongoingCommand && cmd->id() != m_ongoingCommand->uid())
    {
        rollback();
    }

    if(!m_ongoingCommand)
    {
        cmd->enableMerging();
        //m_lockedObject = ObjectPath::pathFromObject(objectToLock);
        //lock_impl();
        m_ongoingCommand = cmd;
        m_ongoingCommand->redo();
    }
    else
    {
        m_ongoingCommand->undo();
        m_ongoingCommand->mergeWith(cmd);
        m_ongoingCommand->redo();
        delete cmd;
    }
}

void OngoingCommandManager::finish()
{
    if(m_ongoingCommand)
    {
        //unlock_impl();

        m_ongoingCommand->undo();
        m_ongoingCommand->disableMerging();

        auto cmd = m_ongoingCommand;
        m_ongoingCommand = nullptr;
        m_commandQueue->pushAndEmit(cmd);
    }
}


void OngoingCommandManager::rollback()
{
    if(m_ongoingCommand)
    {
        //unlock_impl();
        auto cmd = m_ongoingCommand;
        m_ongoingCommand = nullptr;

        cmd->undo();
        delete cmd;
    }
}


OngoingCommandManager::~OngoingCommandManager()
{
    delete m_ongoingCommand;
}
