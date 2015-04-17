#include "NetworkClientDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.hpp>
#include <Repartition/session/ClientSession.hpp>
#include <Repartition/client/RemoteClient.hpp>
#include <Repartition/session/ClientSessionBuilder.hpp>
#include <Serialization/MessageMapper.hpp>

#include <iscore/presenter/PresenterInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>
#include "NetworkControl.hpp"
#include "settings_impl/NetworkSettingsModel.hpp"



NetworkDocumentClientPlugin::NetworkDocumentClientPlugin(ClientSession* s,
                                                         NetworkControl *control,
                                                         iscore::Document *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_session{s},
    m_control{control},
    m_document{doc}
{
    /////////////////////////////////////////////////////////////////////////////
    /// To the master
    /////////////////////////////////////////////////////////////////////////////
    connect(&m_document->commandStack(), &iscore::CommandStack::localCommand,
            this, [=] (iscore::SerializableCommand* cmd)
    {
        m_session->master()->sendMessage(
                    m_session->makeMessage("/command",
                                           cmd->parentName(),
                                           cmd->name(),
                                           cmd->serialize()));
    });

    // Undo-redo
    connect(&m_document->commandStack(), &iscore::CommandStack::localUndo,
            this, [&] ()
    { m_session->master()->sendMessage(m_session->makeMessage("/undo")); });
    connect(&m_document->commandStack(), &iscore::CommandStack::localRedo,
            this, [&] ()
    { m_session->master()->sendMessage(m_session->makeMessage("/redo")); });

    // TODO : messages : peut-être utiliser des tuples en tant que structures ?
    // Cela permettrait de spécifier les types proprement ?
    // Lock-unlock
    connect(&m_document->locker(), &iscore::ObjectLocker::lock,
            this, [&] (QByteArray arr)
    { qDebug() << "client send lock"; m_session->master()->sendMessage(m_session->makeMessage("/lock", arr)); });
    connect(&m_document->locker(), &iscore::ObjectLocker::unlock,
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
        QString parentName; QString name; QByteArray data;
        QDataStream s{m.data};
        s >> parentName >> name >> data;

        m_document->commandStack().redoAndPushQuiet(
                    iscore::IPresenter::instantiateUndoCommand(parentName, name, data));
    });

    s->mapper().addHandler("/undo", [&] (NetworkMessage)
    { m_document->commandStack().undoQuiet(); });

    s->mapper().addHandler("/redo", [&] (NetworkMessage)
    { m_document->commandStack().redoQuiet(); });

    s->mapper().addHandler("/lock", [&] (NetworkMessage m)
    {
        QDataStream s{m.data};
        QByteArray data;
        s >> data;
        m_document->locker().on_lock(data);
    });

    s->mapper().addHandler("/unlock", [&] (NetworkMessage m)
    {
        QDataStream s{m.data};
        QByteArray data;
        s >> data;
        m_document->locker().on_unlock(data);
    });
}

bool NetworkDocumentClientPlugin::canMakeMetadata(const QString &)
{
    qDebug() << Q_FUNC_INFO;
    return false;
}

QVariant NetworkDocumentClientPlugin::makeMetadata(const QString &)
{
    qDebug() << Q_FUNC_INFO;
    return QVariant{};
}

