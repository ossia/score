#include "NetworkClientDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.hpp>
#include <Repartition/session/ClientSessionBuilder.hpp>
#include <Serialization/MessageMapper.hpp>

#include <core/document/DocumentPresenter.hpp>
#include "NetworkApplicationPlugin.hpp"
#include "settings_impl/NetworkSettingsModel.hpp"
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentContext.hpp>


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
                    m_session->makeMessage("/command",
                                           cmd->parentKey(),
                                           cmd->key(),
                                           cmd->serialize()));
    });

    // Undo-redo
    con(m_document->commandStack(), &iscore::CommandStack::localUndo,
            this, [&] ()
    { m_session->master()->sendMessage(m_session->makeMessage("/undo")); });
    con(m_document->commandStack(), &iscore::CommandStack::localRedo,
            this, [&] ()
    { m_session->master()->sendMessage(m_session->makeMessage("/redo")); });

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
    s->mapper().addHandler("/command",
    [&] (NetworkMessage m)
    {
        CommandParentFactoryKey parentName;
        CommandFactoryKey name;
        QByteArray data;
        QDataStream stream{m.data};
        stream >> parentName >> name >> data;

        m_document->commandStack().redoAndPushQuiet(
                    m_document->context().app.components.instantiateUndoCommand(parentName, name, data));
    });

    s->mapper().addHandler("/undo", [&] (NetworkMessage)
    { m_document->commandStack().undoQuiet(); });

    s->mapper().addHandler("/redo", [&] (NetworkMessage)
    { m_document->commandStack().redoQuiet(); });

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
