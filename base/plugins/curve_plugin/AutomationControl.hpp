#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"

class AutomationControl : public iscore::PluginControlInterface
{
    public:
        AutomationControl (QObject* parent);
        virtual ~AutomationControl() = default;

        virtual void populateMenus (iscore::MenubarManager*) override { }
        virtual void populateToolbars() override { }
        virtual void setPresenter (iscore::Presenter*) override { }

        virtual iscore::SerializableCommand* instantiateUndoCommand (const QString& name,
                const QByteArray& data) override;
};
