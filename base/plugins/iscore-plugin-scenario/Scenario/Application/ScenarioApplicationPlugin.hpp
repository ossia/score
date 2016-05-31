#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ToolMenuActions.hpp>
#include <Scenario/Application/Menus/TransportActions.hpp>

#include <QVector>
#include <vector>
#include <Process/Layer/LayerContextMenu.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Execution/ScenarioExecution.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Process {
class LayerPresenter;
class ProcessFocusManager;
}
namespace iscore {
class Document;
struct OrderedToolbar;
}  // namespace iscore

class QAction;
namespace Scenario {
class ObjectMenuActions;
class ScenarioActions;
class TemporalScenarioPresenter;
class ToolMenuActions;
class ScenarioModel;
class ScenarioInterface;
class StateModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
        Q_OBJECT
        friend class ScenarioContextMenuManager;
    public:
        ScenarioApplicationPlugin(
                const iscore::GUIApplicationContext& app);
        ~ScenarioApplicationPlugin();

        GUIElements makeGUIElements() override;

        TemporalScenarioPresenter* focusedPresenter() const;

        void reinit_tools();

        Scenario::EditionSettings& editionSettings()
        { return m_editionSettings; }

        Process::ProcessFocusManager* processFocusManager() const;
        Process::LayerContextMenuManager& layerContextMenuRegistrar()
        { return m_layerCtxMenuManager; }
        const Process::LayerContextMenuManager& layerContextMenuRegistrar() const
        { return m_layerCtxMenuManager; }

        Scenario::ScenarioExecution& execution()
        { return m_execution; }

    signals:
        void keyPressed(int);
        void keyReleased(int);

    protected:
        void prepareNewDocument() override;

        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;

        void on_activeWindowChanged() override;

    private:
        QMetaObject::Connection m_focusConnection, m_defocusConnection, m_contextMenuConnection;
        Scenario::EditionSettings m_editionSettings;
        Process::LayerContextMenuManager m_layerCtxMenuManager;
        ScenarioExecution m_execution;

        ObjectMenuActions m_objectActions{this};
        ToolMenuActions m_toolActions{this};
        TransportActions m_transportActions{context};

        QAction *m_selectAll{};
        QAction *m_deselectAll{};

        void on_presenterFocused(Process::LayerPresenter* lm);
        void on_presenterDefocused(Process::LayerPresenter* lm);
};
}
