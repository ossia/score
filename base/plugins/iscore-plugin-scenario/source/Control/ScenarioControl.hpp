#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"
#include "Menus/Plugin/ScenarioContextMenuPluginList.hpp"
#include "Document/BaseElement/ProcessFocusManager.hpp"

#include <Commands/Scenario/Displacement/MoveEventList.hpp>

#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"

class QActionGroup;
class ScenarioModel;
class ScenarioStateMachine;
class TemporalScenarioPresenter;

class ObjectMenuActions;
class ToolMenuActions;
class AbstractMenuActions;

// TODO Moveme
struct ScenarioRecordInitData
{
        LayerPresenter* presenter{};
        QPoint point;
};
Q_DECLARE_METATYPE(ScenarioRecordInitData)


class ScenarioControl : public iscore::PluginControlInterface
{
        Q_OBJECT
    public:
        ScenarioControl(iscore::Presenter* pres);

        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual QList<iscore::OrderedToolbar> makeToolbars() override;

        virtual iscore::SerializableCommand* instantiateUndoCommand(
                const QString& name,
                const QByteArray& data) override;

        ProcessList* processList()
        { return &m_processList; }

        MoveEventList* moveEventList()
        { return &m_moveEventList; }

        QVector<AbstractMenuActions*>& pluginActions()
        { return m_pluginActions; }

        const ScenarioModel* focusedScenarioModel() const;
        TemporalScenarioPresenter* focusedPresenter() const;

        const ExpandMode& expandMode() const
        { return m_expandMode; }

        void setExpandMode(ExpandMode e)
        { m_expandMode = e; }

        void reinit_tools();

    signals:
        void keyPressed(int);
        void keyReleased(int);

        void startRecording(ScenarioModel&, ScenarioPoint);
        void stopRecording();

    public slots:
        void createContextMenu(const QPoint &);

    protected:
        virtual void on_documentChanged() override;

    private:
        ExpandMode m_expandMode{ExpandMode::Scale};
        ProcessList m_processList;
        MoveEventList m_moveEventList;

        QMetaObject::Connection m_focusConnection, m_defocusConnection;

        ObjectMenuActions* m_objectAction;
        ToolMenuActions* m_toolActions;
        QVector<AbstractMenuActions*> m_pluginActions;

        QAction *m_selectAll{};
        QAction *m_deselectAll{};


        ProcessFocusManager* processFocusManager() const;
        void on_presenterFocused(LayerPresenter* lm);
        void on_presenterDefocused(LayerPresenter* lm);
        void setupCommands();
};
