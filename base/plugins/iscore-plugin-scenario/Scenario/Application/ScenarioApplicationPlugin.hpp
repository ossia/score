#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <Process/ProcessList.hpp>
#include "Menus/Plugin/ScenarioContextMenuPluginList.hpp"
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include "Menus/ContextMenuDispatcher.hpp"
class QActionGroup;
namespace Scenario { class ScenarioModel; }
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

class ScenarioApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
        Q_OBJECT
        friend class ScenarioContextMenuManager;
    public:
        ScenarioApplicationPlugin(iscore::Application& app);

        void populateMenus(iscore::MenubarManager*) override;
        std::vector<iscore::OrderedToolbar> makeToolbars() override;
        std::vector<QAction*> actions() override;

        QVector<ScenarioActions*>& pluginActions()
        { return m_pluginActions; }

        const Scenario::ScenarioModel* focusedScenarioModel() const;
        TemporalScenarioPresenter* focusedPresenter() const;

        void reinit_tools();

        Scenario::EditionSettings& editionSettings()
        { return m_editionSettings; }

        ProcessFocusManager* processFocusManager() const;

    signals:
        void keyPressed(int);
        void keyReleased(int);

        void startRecording(Scenario::ScenarioModel&, Scenario::Point);
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

        void on_presenterFocused(LayerPresenter* lm);
        void on_presenterDefocused(LayerPresenter* lm);
};
