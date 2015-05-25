#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"

class AutomationControl : public iscore::PluginControlInterface
{
    public:
        AutomationControl(iscore::Presenter* pres);
        virtual ~AutomationControl() = default;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                const QByteArray& data) override;
};
