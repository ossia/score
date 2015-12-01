#include <Serialization/MessageMapper.hpp>
#include <boost/optional/optional.hpp>
#include <core/document/DocumentContext.hpp>
#include <QByteArray>
#include <QDataStream>
#include <algorithm>

#include "NetworkMasterDocumentPlugin.hpp"
#include "Serialization/NetworkMessage.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/command/CommandStack.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <core/document/Document.hpp>
#include "session/MasterSession.hpp"

class Client;

MasterNetworkPolicy::MasterNetworkPolicy(MasterSession* s,
                                         iscore::DocumentContext& c):
    m_session{s}
{
    auto& stack = c.document.commandStack();

    /////////////////////////////////////////////////////////////////////////////
    /// From the master to the clients
    /////////////////////////////////////////////////////////////////////////////
    con(stack, &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->broadcast(
                    m_session->makeMessage("/command/new",iscore::CommandData{*cmd}));
    });

    // Undo-redo
    con(stack, &iscore::CommandStack::localUndo,
            this, [&] ()
    { m_session->broadcast(m_session->makeMessage("/command/undo")); });
    con(stack, &iscore::CommandStack::localRedo,
            this, [&] ()
    { m_session->broadcast(m_session->makeMessage("/command/redo")); });
    con(stack, &iscore::CommandStack::localIndexChanged,
            this, [&] (int32_t idx)
    {
        m_session->broadcast(m_session->makeMessage("/command/index", idx));
    });

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
    s->mapper().addHandler("/command/new", [&] (NetworkMessage m)
    {
        iscore::CommandData cmd;
        Visitor<Writer<DataStream>> writer{m.data};
        writer.writeTo(cmd);

        stack.redoAndPushQuiet(
                    c.app.components.instantiateUndoCommand(cmd));


        m_session->transmit(Id<Client>(m.clientId), m);
    });

    // Undo-redo
    s->mapper().addHandler("/command/undo", [&] (NetworkMessage m)
    {
        stack.undoQuiet();
        m_session->transmit(Id<Client>(m.clientId), m);
    });
    s->mapper().addHandler("/command/redo", [&] (NetworkMessage m)
    {
        stack.redoQuiet();
        m_session->transmit(Id<Client>(m.clientId), m);
    });

    s->mapper().addHandler("/command/index", [&] (NetworkMessage m)
    {
        QDataStream stream{m.data};
        int32_t idx;
        stream >> idx;
        stack.setIndexQuiet(idx);
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

