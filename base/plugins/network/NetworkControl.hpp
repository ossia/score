#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class ClientSessionBuilder;
class ClientSession;

#ifdef USE_ZEROCONF
class ZeroconfBrowser;
#endif

class NetworkControl : public iscore::PluginControlInterface
{
        Q_OBJECT

    public:
        NetworkControl();
        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars() override;

    public slots:
        void setupClientConnection(QString ip, int port);
        void on_sessionBuilt(ClientSessionBuilder* sessionBuilder, ClientSession* builtSession);

    private:
        iscore::Presenter* m_presenter {};
        ClientSessionBuilder* m_sessionBuilder{};

#ifdef USE_ZEROCONF
        ZeroconfBrowser* m_zeroconfBrowser{};
#endif
};
