#include <Serialization/MessageMapper.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <algorithm>
#include <iscore/command/CommandData.hpp>
#include "NetworkClientDocumentPlugin.hpp"
#include "Serialization/NetworkMessage.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/Todo.hpp>
#include "session/../client/RemoteClient.hpp"
#include "session/ClientSession.hpp"


ClientNetworkPolicy::ClientNetworkPolicy(
        ClientSession* s,
        iscore::Document *doc):
    m_session{s},
    m_document{doc}
{
    /////////////////////////////////////////////////////////////////////////////
    /// To the master
    /////////////////////////////////////////////////////////////////////////////
    con(m_document->commandStack(), &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->master()->sendMessage(
                    m_session->makeMessage("/command/new", iscore::CommandData{*cmd}));
    });

    // Undo-redo
    con(m_document->commandStack(), &iscore::CommandStack::localUndo,
            this, [&] ()
    { m_session->master()->sendMessage(m_session->makeMessage("/command/undo")); });
    con(m_document->commandStack(), &iscore::CommandStack::localRedo,
            this, [&] ()
    { m_session->master()->sendMessage(m_session->makeMessage("/command/redo")); });
    con(m_document->commandStack(), &iscore::CommandStack::localIndexChanged,
            this, [&] (int32_t idx)
    { m_session->master()->sendMessage(m_session->makeMessage("/command/index", idx)); });

    // TODO : messages : peut-être utiliser des tuples en tant que structures ?
    // Cela permettrait de spécifier les types proprement ?
    // Lock-unlock
    con(m_document->locker(), &iscore::ObjectLocker::lock,
            this, [&] (QByteArray arr)
    { qDebug() << "client send lock"; m_session->master()->sendMessage(m_session->makeMessage("/lock", arr)); });
    con(m_document->locker(), &iscore::ObjectLocker::unlock,
            this, [&] (QByteArray arr)
    { qDebug() << "client send unlock"; m_session->master()->sendMessage(m_session->makeMessage("/unlock", arr)); });


    /////////////////////////////////////////////////////////////////////////////
    /// From the master
    /////////////////////////////////////////////////////////////////////////////
    // - command comes from the master
    //   -> apply it to the computer only
    s->mapper().addHandler("/command/new",
    [&] (NetworkMessage m)
    {
        iscore::CommandData cmd;
        Visitor<Writer<DataStream>> writer{m.data};
        writer.writeTo(cmd);

        m_document->commandStack().redoAndPushQuiet(
                    m_document->context().app.components.instantiateUndoCommand(cmd));
    });

    s->mapper().addHandler("/command/undo", [&] (NetworkMessage)
    { m_document->commandStack().undoQuiet(); });

    s->mapper().addHandler("/command/redo", [&] (NetworkMessage)
    { m_document->commandStack().redoQuiet(); });

    s->mapper().addHandler("/command/index", [&] (NetworkMessage m)
    {
        QDataStream stream{m.data};
        int32_t idx;
        stream >> idx;
        m_document->commandStack().setIndexQuiet(idx);
    });

    s->mapper().addHandler("/lock", [&] (NetworkMessage m)
    {
        QDataStream stream{m.data};
        QByteArray data;
        stream >> data;
        m_document->locker().on_lock(data);
    });

    s->mapper().addHandler("/unlock", [&] (NetworkMessage m)
    {
        QDataStream stream{m.data};
        QByteArray data;
        stream >> data;
        m_document->locker().on_unlock(data);
    });
}
