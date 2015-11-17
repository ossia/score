#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <Process/ProcessList.hpp>
#include "Menus/Plugin/ScenarioContextMenuPluginList.hpp"
#include <Scenario/Document/BaseElement/ProcessFocusManager.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>
#include <Scenario/Control/ScenarioEditionSettings.hpp>
#include "Menus/ContextMenuDispatcher.hpp"
class QActionGroup;
class ScenarioModel;
class SlotPresenter;
class TemporalScenarioPresenter;

class ObjectMenuActions;
class ToolMenuActions;
class ScenarioActions;

// TODO Moveme
struct ScenarioRecordInitData
{
        ScenarioRecordInitData() {}
        ScenarioRecordInitData(const LayerPresenter* lp, QPointF p):
            presenter{lp},
            point{p}
        {
        }

        const LayerPresenter* presenter{};
        QPointF point;
};
Q_DECLARE_METATYPE(ScenarioRecordInitData)

class ScenarioControl final : public iscore::PluginControlInterface
{
        Q_OBJECT
        friend class ScenarioContextMenuManager;
    public:
        ScenarioControl(iscore::Application& app);

        void populateMenus(iscore::MenubarManager*) override;
        QList<iscore::OrderedToolbar> makeToolbars() override;
        QList<QAction*> actions() override;

        QVector<ScenarioActions*>& pluginActions()
        { return m_pluginActions; }

        const ScenarioModel* focusedScenarioModel() const;
        TemporalScenarioPresenter* focusedPresenter() const;

        void reinit_tools();

        Scenario::EditionSettings& editionSettings()
        { return m_editionSettings; }

    signals:
        void keyPressed(int);
        void keyReleased(int);

        void startRecording(ScenarioModel&, Scenario::Point);
        void stopRecording();

    protected:
        void prepareNewDocument() override;

        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;

    private:
        void initColors();


        QMetaObject::Connection m_focusConnection, m_defocusConnection, m_contextMenuConnection;
        Scenario::EditionSettings m_editionSettings;

        ObjectMenuActions* m_objectAction{};
        ToolMenuActions* m_toolActions{};
        QVector<ScenarioActions*> m_pluginActions;

        QAction *m_selectAll{};
        QAction *m_deselectAll{};

        ProcessFocusManager* processFocusManager() const;
        void on_presenterFocused(LayerPresenter* lm);
        void on_presenterDefocused(LayerPresenter* lm);
};
