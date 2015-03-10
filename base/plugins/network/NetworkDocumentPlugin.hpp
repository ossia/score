#pragma once

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

#include <Repartition/session/ConnectionData.hpp>
#include "remote/RemoteActionEmitter.hpp"
#include "remote/RemoteActionReceiver.hpp"

class NetworkControl;
namespace iscore
{
    class Document;
}
class NetworkDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        NetworkDocumentPlugin(NetworkControl* control, iscore::Document* doc);

        void handle__document_ask(osc::ReceivedMessageArgumentStream args);
        void handle__document_receive(osc::ReceivedMessageArgumentStream args);

    signals:
        void loadFromNetwork(QByteArray);

    public slots:
        void setupMasterSession();
        void setupClientSession(ConnectionData d);

        void createZeroconfSelectionDialog();

        void commandPush(iscore::SerializableCommand* cmd);
        void on_commandReceived(QString par_name,
                                QString cmd_name,
                                QByteArray data);



    private:
        void setupConnections();

        NetworkControl* m_control{};
        iscore::Document* m_document{};
        std::unique_ptr<Session> m_networkSession;
        std::unique_ptr<RemoteActionEmitter> m_emitter;
        std::unique_ptr<RemoteActionReceiver> m_receiver;
};
