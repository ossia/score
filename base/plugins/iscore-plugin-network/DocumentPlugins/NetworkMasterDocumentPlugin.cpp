#include "NetworkMasterDocumentPlugin.hpp"

#include <Serialization/MessageMapper.hpp>

#include <iscore/presenter/PresenterInterface.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>

#include "NetworkControl.hpp"
#include "settings_impl/NetworkSettingsModel.hpp"

MasterNetworkPolicy::MasterNetworkPolicy(MasterSession* s,
                                         iscore::CommandStack& stack,
                                         iscore::ObjectLocker& locker):
    m_session{s}
{
    /////////////////////////////////////////////////////////////////////////////
    /// From the master to the clients
    /////////////////////////////////////////////////////////////////////////////
    connect(&stack, &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->broadcast(
                    m_session->makeMessage("/command",
                                           cmd->parentName(),
                                           cmd->name(),
                                           cmd->serialize()));
    });

    // Undo-redo
    connect(&stack, &iscore::CommandStack::localUndo,
            this, [&] ()
    { m_session->broadcast(m_session->makeMessage("/undo")); });
    connect(&stack, &iscore::CommandStack::localRedo,
            this, [&] ()
    { m_session->broadcast(m_session->makeMessage("/redo")); });

    // Lock - unlock
    connect(&locker, &iscore::ObjectLocker::lock,
            this, [&] (QByteArray arr)
    { m_session->broadcast(m_session->makeMessage("/lock", arr)); });
    connect(&locker, &iscore::ObjectLocker::unlock,
            this, [&] (QByteArray arr)
    { m_session->broadcast(m_session->makeMessage("/unlock", arr)); });


    /////////////////////////////////////////////////////////////////////////////
    /// From a client to the master and the other clients
    /////////////////////////////////////////////////////////////////////////////
    s->mapper().addHandler("/command", [&] (NetworkMessage m)
    {
        QString parentName; QString name; QByteArray data;
        QDataStream s{m.data};
        s >> parentName >> name >> data;

        stack.redoAndPushQuiet(
                    iscore::IPresenter::instantiateUndoCommand(parentName, name, data));


        m_session->transmit(id_type<Client>(m.clientId), m);
    });

    // TODO aspect-orientation would *really* help here.
    // Undo-redo
    s->mapper().addHandler("/undo", [&] (NetworkMessage m)
    {
        stack.undoQuiet();
        m_session->transmit(id_type<Client>(m.clientId), m);
    });
    s->mapper().addHandler("/redo", [&] (NetworkMessage m)
    {
        stack.redoQuiet();
        m_session->transmit(id_type<Client>(m.clientId), m);
    });

    // Lock-unlock
    s->mapper().addHandler("/lock", [&] (NetworkMessage m)
    {
        QDataStream s{m.data};
        QByteArray data;
        s >> data;
        locker.on_lock(data);
        m_session->transmit(id_type<Client>(m.clientId), m);
    });

    s->mapper().addHandler("/unlock", [&] (NetworkMessage m)
    {
        QDataStream s{m.data};
        QByteArray data;
        s >> data;
        locker.on_unlock(data);
        m_session->transmit(id_type<Client>(m.clientId), m);
    });
}

