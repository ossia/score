#include <Serialization/MessageMapper.hpp>
#include <boost/optional/optional.hpp>
#include <core/document/DocumentContext.hpp>
#include <qbytearray.h>
#include <qdatastream.h>
#include <algorithm>

#include "NetworkMasterDocumentPlugin.hpp"
#include "Serialization/NetworkMessage.hpp"
#include "core/application/ApplicationComponents.hpp"
#include "core/application/ApplicationContext.hpp"
#include "core/command/CommandStack.hpp"
#include "iscore/command/SerializableCommand.hpp"
#include "iscore/locking/ObjectLocker.hpp"
#include "iscore/plugins/customfactory/StringFactoryKey.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/SettableIdentifier.hpp"
#include "iscore/tools/Todo.hpp"
#include "session/MasterSession.hpp"

class Client;

MasterNetworkPolicy::MasterNetworkPolicy(MasterSession* s,
                                         iscore::DocumentContext& c):
    m_session{s}
{
    /////////////////////////////////////////////////////////////////////////////
    /// From the master to the clients
    /////////////////////////////////////////////////////////////////////////////
    con(c.commandStack, &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->broadcast(
                    m_session->makeMessage("/command",
                                           cmd->parentKey(),
                                           cmd->key(),
                                           cmd->serialize()));
    });

    // Undo-redo
    con(c.commandStack, &iscore::CommandStack::localUndo,
            this, [&] ()
    { m_session->broadcast(m_session->makeMessage("/undo")); });
    con(c.commandStack, &iscore::CommandStack::localRedo,
            this, [&] ()
    { m_session->broadcast(m_session->makeMessage("/redo")); });

    // Lock - unlock
    con(c.objectLocker, &iscore::ObjectLocker::lock,
            this, [&] (QByteArray arr)
    { m_session->broadcast(m_session->makeMessage("/lock", arr)); });
    con(c.objectLocker, &iscore::ObjectLocker::unlock,
            this, [&] (QByteArray arr)
    { m_session->broadcast(m_session->makeMessage("/unlock", arr)); });


    /////////////////////////////////////////////////////////////////////////////
    /// From a client to the master and the other clients
    /////////////////////////////////////////////////////////////////////////////
    s->mapper().addHandler("/command", [&] (NetworkMessage m)
    {
        CommandParentFactoryKey parentName;
        CommandFactoryKey name;
        QByteArray data;
        QDataStream stream{m.data};
        stream >> parentName >> name >> data;

        c.commandStack.redoAndPushQuiet(
                    c.app.components.instantiateUndoCommand(parentName, name, data));


        m_session->transmit(Id<Client>(m.clientId), m);
    });

    // Undo-redo
    s->mapper().addHandler("/undo", [&] (NetworkMessage m)
    {
        c.commandStack.undoQuiet();
        m_session->transmit(Id<Client>(m.clientId), m);
    });
    s->mapper().addHandler("/redo", [&] (NetworkMessage m)
    {
        c.commandStack.redoQuiet();
        m_session->transmit(Id<Client>(m.clientId), m);
    });

    // Lock-unlock
    s->mapper().addHandler("/lock", [&] (NetworkMessage m)
    {
        QDataStream stream{m.data};
        QByteArray data;
        stream >> data;
        c.objectLocker.on_lock(data);
        m_session->transmit(Id<Client>(m.clientId), m);
    });

    s->mapper().addHandler("/unlock", [&] (NetworkMessage m)
    {
        QDataStream stream{m.data};
        QByteArray data;
        stream >> data;
        c.objectLocker.on_unlock(data);
        m_session->transmit(Id<Client>(m.clientId), m);
    });
}

