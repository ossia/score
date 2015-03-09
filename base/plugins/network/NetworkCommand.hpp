#pragma once
#include <iscore/tools/utilsCPP11.hpp>
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

#include <memory>


#include <iscore/command/Command.hpp>

class NetworkControl : public iscore::PluginControlInterface
{
        Q_OBJECT

    public:
        NetworkControl();
        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars() override;


    protected:
        virtual void on_newDocument(iscore::Document* doc) override;

    private:
        iscore::Presenter* m_presenter {};
};
