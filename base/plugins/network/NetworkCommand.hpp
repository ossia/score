#pragma once
#include <core/tools/utilsCPP11.hpp>
#include <plugin_interface/plugincontrol/PluginControlInterface.hpp>

#include <Repartition/session/Session.h>
#include <Repartition/session/ConnectionData.hpp>
#include <memory>

#include "remote/RemoteActionEmitter.hpp"
#include "remote/RemoteActionReceiver.hpp"


#include <core/presenter/command/Command.hpp>

class NetworkControl : public iscore::PluginControlInterface
{
        Q_OBJECT

    public:
        NetworkControl();
        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars() override;


    protected:
        virtual void on_newDocument(iscore::Document* doc);

    private:
        iscore::Presenter* m_presenter {};
};
