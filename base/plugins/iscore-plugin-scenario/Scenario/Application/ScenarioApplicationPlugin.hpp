#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

#include <QVector>
#include <vector>
#include <Scenario/Palette/ScenarioPoint.hpp>

class LayerPresenter;
class ObjectMenuActions;
class ProcessFocusManager;
class QAction;
class ScenarioActions;
class TemporalScenarioPresenter;
class ToolMenuActions;
namespace Scenario {
class ScenarioModel;
}
namespace iscore {

class Document;
class MenubarManager;
struct OrderedToolbar;
}  // namespace iscore


class ScenarioApplicationPlugin final : public QObject, public iscore::GUIApplicationContextPlugin
{
        Q_OBJECT
        friend class ScenarioContextMenuManager;
    public:
        ScenarioApplicationPlugin(const iscore::ApplicationContext& app);

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
