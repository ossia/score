#pragma once
#include <iscore/tools/utilsCPP11.hpp>
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

#include <memory>


#include <iscore/command/Command.hpp>
#include "Repartition/session/ConnectionData.hpp"
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

    protected:
        virtual void on_newDocument(iscore::Document* doc) override;

    private:
        iscore::Presenter* m_presenter {};
        ClientSessionBuilder* m_sessionBuilder{};
};
