#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.hpp>
#include <Repartition/session/ClientSession.hpp>
#include <Repartition/client/RemoteClient.hpp>
#include <Repartition/session/ClientSessionBuilder.h>


#include <iscore/presenter/PresenterInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>
#include "NetworkCommand.hpp"
#include "settings_impl/NetworkSettingsModel.hpp"



NetworkDocumentClientPlugin::NetworkDocumentClientPlugin(ClientSession* s,
                                                         NetworkControl *control,
                                                         iscore::Document *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_session{s},
    m_control{control},
    m_document{doc}
{
    // Multiple cases :
    // - command comes from the local computer
    //   -> send it to the master
    connect(&m_document->commandStack(), &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->master()->sendMessage(
                    m_session->makeMessage("/command",
                                           cmd->parentName(),
                                           cmd->name(),
                                           cmd->serialize()));
    });
    connect(&m_document->commandStack(), &iscore::CommandStack::localUndo,
            this, [&] ()
    {
        m_session->master()->sendMessage(m_session->makeMessage("/undo"));
    });
    connect(&m_document->commandStack(), &iscore::CommandStack::localRedo,
            this, [&] ()
    {
        m_session->master()->sendMessage(m_session->makeMessage("/redo"));
    });


    // - command comes from the master
    //   -> apply it to the computer only
    s->mapper().addHandler("/command",
    [&] (NetworkMessage m)
    {
        QString parentName; QString name; QByteArray data;
        QDataStream s{m.data};
        s >> parentName >> name >> data;

        m_document->commandStack().redoAndPushQuiet(
                    iscore::IPresenter::instantiateUndoCommand(parentName, name, data));
    });

    s->mapper().addHandler("/undo",
    [&] (NetworkMessage)
    {
        m_document->commandStack().undoQuiet();
    });

    s->mapper().addHandler("/redo",
    [&] (NetworkMessage)
    {
        m_document->commandStack().redoQuiet();
    });
}


NetworkDocumentMasterPlugin::NetworkDocumentMasterPlugin(MasterSession* s,
                                                         NetworkControl* control,
                                                         iscore::Document* doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_session{s},
    m_control{control},
    m_document{doc}
{
    // Multiple cases :
    // - command comes from the local computer (the master)
    //   -> broadcast

    connect(&m_document->commandStack(), &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->broadcast(
                    m_session->makeMessage("/command",
                                           cmd->parentName(),
                                           cmd->name(),
                                           cmd->serialize()));
    });
    connect(&m_document->commandStack(), &iscore::CommandStack::localUndo,
            this, [&] ()
    {
        m_session->broadcast(m_session->makeMessage("/undo"));
    });
    connect(&m_document->commandStack(), &iscore::CommandStack::localRedo,
            this, [&] ()
    {
        m_session->broadcast(m_session->makeMessage("/redo"));
    });


    // - command comes from a client
    //   -> transmit to all but the client
    s->mapper().addHandler("/command", [&] (NetworkMessage m)
    {
        qDebug() << Q_FUNC_INFO;
        QString parentName; QString name; QByteArray data;
        QDataStream s{m.data};
        s >> parentName >> name >> data;

        m_document->commandStack().redoAndPushQuiet(
                    iscore::IPresenter::instantiateUndoCommand(parentName, name, data));


        m_session->transmit(id_type<RemoteClient>(m.clientId), m);
    });
    s->mapper().addHandler("/undo", [&] (NetworkMessage m)
    {
        qDebug() << Q_FUNC_INFO;
        m_document->commandStack().undoQuiet();
        m_session->transmit(id_type<RemoteClient>(m.clientId), m);
    });
    s->mapper().addHandler("/redo", [&] (NetworkMessage m)
    {
        qDebug() << Q_FUNC_INFO;
        m_document->commandStack().redoQuiet();
        m_session->transmit(id_type<RemoteClient>(m.clientId), m);
    });

}
