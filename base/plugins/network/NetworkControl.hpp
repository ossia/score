#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class ClientSessionBuilder;
class ClientSession;

class NetworkControl : public iscore::PluginControlInterface
{
        Q_OBJECT

    public:
        NetworkControl();
        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars() override;

        void setupClientConnection(QString ip, int port);

    public slots:
        void on_sessionBuilt(ClientSessionBuilder* sessionBuilder, ClientSession* builtSession);

    private:
        iscore::Presenter* m_presenter {};
        ClientSessionBuilder* m_sessionBuilder{};
};
