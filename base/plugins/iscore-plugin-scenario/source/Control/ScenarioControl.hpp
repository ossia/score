#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"
#include "Menus/Plugin/ScenarioContextMenuPluginList.hpp"
#include "Document/BaseElement/ProcessFocusManager.hpp"

class QActionGroup;
class ScenarioModel;
class ScenarioStateMachine;
class TemporalScenarioPresenter;

class ObjectMenuActions;
class ToolMenuActions;
class AbstractMenuActions;
class ScenarioControl : public iscore::PluginControlInterface
{
    public:
        ScenarioControl(iscore::Presenter* pres);

        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual QList<OrderedToolbar> makeToolbars() override;

        virtual iscore::SerializableCommand* instantiateUndoCommand(
                const QString& name,
                const QByteArray& data) override;

        ProcessList* processList()
        { return &m_processList; }

        QVector<AbstractMenuActions*>& pluginActions()
        { return m_pluginActions; }

        const ScenarioModel* focusedScenarioModel() const;
        TemporalScenarioPresenter* focusedPresenter() const;

        const ExpandMode& expandMode() const
        { return m_expandMode; }

        void setExpandMode(ExpandMode e)
        { m_expandMode = e; }

    public slots:
        void createContextMenu(const QPoint &);

    protected:
        virtual void on_documentChanged() override;

    private:
        ExpandMode m_expandMode;
        ProcessList m_processList;

        QMetaObject::Connection m_focusConnection, m_defocusConnection;

        ObjectMenuActions* m_objectAction;
        ToolMenuActions* m_toolActions;
        QVector<AbstractMenuActions*> m_pluginActions;

        QAction *m_selectAll{};
        QAction *m_deselectAll{};


        ProcessFocusManager* processFocusManager() const;
        void on_presenterFocused(ProcessPresenter* lm);
        void on_presenterDefocused(ProcessPresenter* lm);
        void setupCommands();
};
