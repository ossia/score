#pragma once
#include <plugin_interface/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"

class ScenarioControl : public iscore::PluginControlInterface
{
    public:
        ScenarioControl(QObject* parent);

        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars() override;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                const QByteArray& data) override;

        ProcessList* processList()
        {
            return m_processList;
        }

    private:
        ProcessList* m_processList {};
};
