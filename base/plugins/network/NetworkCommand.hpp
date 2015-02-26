#pragma once
#include <core/tools/utilsCPP11.hpp>
#include <interface/plugincontrol/PluginControlInterface.hpp>

#include <Repartition/session/Session.h>
#include <Repartition/session/ConnectionData.hpp>
#include <memory>

#include "remote/RemoteActionEmitter.hpp"
#include "remote/RemoteActionReceiver.hpp"


#include <core/presenter/command/Command.hpp>

class NetworkCommand : public iscore::PluginControlInterface
{
        Q_OBJECT

    public:
        NetworkCommand();
        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars() override;
        virtual void setPresenter(iscore::Presenter*) override;

        // @todo PUT SOMEWHERE ELSE
        void handle__document_ask(osc::ReceivedMessageArgumentStream);
        void handle__document_receive(osc::ReceivedMessageArgumentStream);
    signals:
        void loadFromNetwork(QByteArray);

    public slots:
        void setupMasterSession();
        void setupClientSession(ConnectionData d);

        void createZeroconfSelectionDialog();

        void commandPush(iscore::SerializableCommand*);

        void on_commandReceived(QString, QString, QByteArray);

    private:
        iscore::Presenter* m_presenter {};
        std::unique_ptr<Session> m_networkSession;

    private:
        std::unique_ptr<RemoteActionEmitter> m_emitter;
        std::unique_ptr<RemoteActionReceiver> m_receiver;
};
