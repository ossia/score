#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"
#include "Document/BaseElement/ProcessFocusManager.hpp"

class QActionGroup;
class ScenarioModel;
class ScenarioStateMachine;
class TemporalScenarioPresenter;

class ObjectMenuActions;
class ToolMenuActions;

class ScenarioControl : public iscore::PluginControlInterface
{
    friend class ObjectMenuActions;
    friend class ToolMenuActions;

    public:
        ScenarioControl(iscore::Presenter* pres);

        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual QList<OrderedToolbar> makeToolbars() override;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                const QByteArray& data) override;

        // TODO why not in the plug-in ? (cf. DeviceExplorer)
        // Or - why could not the control be instead the Plugin (what is its point) ?
        ProcessList* processList()
        { return &m_processList; }

    public slots:
        void createContextMenu(const QPoint &);

    protected:
        virtual void on_documentChanged() override;
        const ScenarioModel* focusedScenarioModel() const;

    private:
        ProcessList m_processList;

        QMetaObject::Connection m_focusConnection, m_defocusConnection;

        ObjectMenuActions* m_objectAction;
        ToolMenuActions* m_toolActions;

        QAction *m_selectAll{};
        QAction *m_deselectAll{};

        TemporalScenarioPresenter* focusedPresenter() const;

        ProcessFocusManager* processFocusManager() const;
        void on_presenterFocused(ProcessPresenter* lm);
        void on_presenterDefocused(ProcessPresenter* lm);
};
