#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"

class QActionGroup;
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

    protected:
        virtual void on_newDocument(iscore::Document* doc) override;
        virtual void on_presenterChanged() override;
        virtual void on_documentChanged(iscore::Document* doc) override;

    private:
        ProcessList* m_processList {};
        QActionGroup* m_scenarioActionGroup{};
        QMetaObject::Connection m_toolbarConnection;
};
