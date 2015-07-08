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

        ProcessList* processList()
        { return &m_processList; }
        ScenarioContextMenuPluginList* contextMenuList()
        { return &m_contextMenuList; }

    public slots:
        void createContextMenu(const QPoint &);

    protected:
        virtual void on_documentChanged() override;
        const ScenarioModel* focusedScenarioModel() const;

    private:
        ProcessList m_processList;
        ScenarioContextMenuPluginList m_contextMenuList;


        QMetaObject::Connection m_focusConnection, m_defocusConnection;

        ObjectMenuActions* m_objectAction;
        ToolMenuActions* m_toolActions;

        QAction *m_selectAll{};
        QAction *m_deselectAll{};

        TemporalScenarioPresenter* focusedPresenter() const;

        ProcessFocusManager* processFocusManager() const;
        void on_presenterFocused(ProcessPresenter* lm);
        void on_presenterDefocused(ProcessPresenter* lm);
        void setupCommands();
};
