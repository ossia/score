#include "RemoteActionReceiver.hpp"

RemoteActionReceiver::RemoteActionReceiver(Session* s)
{
}

void RemoteActionReceiver::handle__edit_undo(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId;
    osc::int32 clientId;

    args >> sessionId >> clientId;

    if(sessionId != session()->getId())
    {
        return;
    }

    emit undo();
}

void RemoteActionReceiver::handle__edit_redo(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId;
    osc::int32 clientId;

    args >> sessionId >> clientId;

    if(sessionId != session()->getId())
    {
        return;
    }

    emit redo();
}

void RemoteActionReceiver::handle__edit_lock(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId;
    osc::int32 clientId;

    osc::Blob blob;
    args >> sessionId >> clientId >> blob;

    if(sessionId != session()->getId())
    {
        return;
    }

    emit lock(QByteArray {static_cast<const char*>(blob.data), blob.size});
}

void RemoteActionReceiver::handle__edit_unlock(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId;
    osc::int32 clientId;

    osc::Blob blob;
    args >> sessionId >> clientId >> blob;

    if(sessionId != session()->getId())
    {
        return;
    }

    emit unlock(QByteArray {static_cast<const char*>(blob.data), blob.size});
}

void RemoteActionReceiver::handle__edit_command(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId;
    osc::int32 clientId;
    const char* par_name;
    const char* cmd_name;
    osc::Blob blob;

    args >> sessionId >> clientId >> par_name >> cmd_name >> blob;

    if(sessionId != session()->getId())
    {
        return;
    }

    emit commandReceived(QString {par_name},
                         QString {cmd_name},
                         QByteArray {static_cast<const char*>(blob.data), blob.size});
}
