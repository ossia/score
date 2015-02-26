#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"

class ScenarioControl : public iscore::PluginControlInterface
{
    public:
        ScenarioControl (QObject* parent);

        virtual void populateMenus (iscore::MenubarManager*) override;
        virtual void populateToolbars() override;
        virtual void setPresenter (iscore::Presenter*) override;

        virtual iscore::SerializableCommand* instantiateUndoCommand (const QString& name,
                const QByteArray& data) override;

        ProcessList* processList()
        {
            return m_processList;
        }

    private slots:
        void deselectAll();
        void selectAll();

        // Only if there is device explorer AND curve plugin
        void createCurvesFromAddresses();

    private:
        ProcessList* m_processList {};


};
