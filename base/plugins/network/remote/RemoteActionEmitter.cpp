#include "RemoteActionEmitter.hpp"
#include <Repartition/session/Session.h>
#include <core/presenter/command/SerializableCommand.hpp>

RemoteActionEmitter::RemoteActionEmitter(Session* session) :
    m_session {session}
{

}

void RemoteActionEmitter::sendCommand(iscore::SerializableCommand* cmd)
{
    QByteArray data = cmd->serialize();
    m_session->sendCommand(cmd->parentName().toLatin1().constData(),
                           cmd->name().toLatin1().constData(),
                           data.constData(),
                           data.length());

}

void RemoteActionEmitter::undo()
{
    m_session->sendUndoCommand();
}

void RemoteActionEmitter::redo()
{
    m_session->sendRedoCommand();
}

void RemoteActionEmitter::on_lock(QByteArray arr)
{
    m_session->sendLock(arr);
}

void RemoteActionEmitter::on_unlock(QByteArray arr)
{
    m_session->sendUnlock(arr);
}
