#include "OngoingCommandManager.hpp"
#include <QDebug>
#include <core/interface/document/DocumentInterface.hpp>


using namespace iscore;
// TODO this should be in the iscore namespace ?
CommandDispatcher::CommandDispatcher(QObject* parent):
    QObject{parent},
    m_commandQueue{IDocument::commandQueue(IDocument::documentFromObject(parent))}
{

}

void CommandDispatcher::send(SerializableCommand *cmd)
{
    commandQueue()->pushAndEmit(cmd);
}


// Old documentation. TODO Update it :
/**
 * These slots :
 *   * initiateOngoingCommand,
 *   * continueOngoingCommand,
 *   * validateOngoingCommand,
 *   * undoOngoingCommand
 *
 * Are to be used when a Command takes multiple "steps" that must be
 * checked by the user, and do impact the model.
 * For instance, when resizing an element with the mouse, it is necessary to see
 * the effects of the transformation on the whole model. But it has to be applied only once.
 *
 * The locking is for the network implementation : the specified object will appear "locked"
 * to other users and they won't be able to modify it, in order to prevent conflicts.
 *
 * First, call initiateOngoingCommand with the initial Command (for instance
 * in a mousePressEvent). (Command::mergeWith() must work).
 * Then, keep making new Commands at each "change" (for instance, each mouseMoveEvent) and
 * apply them with continueOngoingCommand.
 *
 * When everything is done (e.g. mouseReleaseEvent), validateOngoingCommand is to be called.
 * If the user wants to cancel his command, for instance by pressing the "Escape" key,
 * call undoOngoingCommand()
 *
 * A signal will be sent in case of validateOngoingCommand, to propagate it to the network.
 *
 */
/*
void initiateOngoingCommand(iscore::SerializableCommand*, QObject* objectToLock);
void continueOngoingCommand(iscore::SerializableCommand*);
void rollbackOngoingCommand();
void validateOngoingCommand();*/

void OngoingCommandDispatcher::send(iscore::SerializableCommand* cmd)
{
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

void OngoingCommandDispatcher::commit()
{
    if(m_ongoingCommand)
    {
        //unlock_impl();

        m_ongoingCommand->undo();
        m_ongoingCommand->disableMerging();

        auto cmd = m_ongoingCommand;
        m_ongoingCommand = nullptr;
        commandQueue()->pushAndEmit(cmd);
    }
}


void OngoingCommandDispatcher::rollback()
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


OngoingCommandDispatcher::~OngoingCommandDispatcher()
{
    delete m_ongoingCommand;
}
