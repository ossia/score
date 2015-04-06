#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"

class ScenarioControl : public iscore::PluginControlInterface
{
    public:
        ScenarioControl(QObject* parent);

        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual void populateToolbars(QToolBar* bar) override;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                const QByteArray& data) override;

        // TODO why not in the plug-in ? (cf. DeviceExplorer)
        // Or - why could not the control be instead the Plugin (what is its point) ?
        ProcessList* processList()
        {
            return m_processList;
        }

    private:
        ProcessList* m_processList {};
};
