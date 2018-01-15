#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ToolMenuActions.hpp>
#include <Scenario/Application/Menus/TransportActions.hpp>

#include <Process/Layer/LayerContextMenu.hpp>
#include <QVector>
#include <Scenario/Execution/ScenarioExecution.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <score_plugin_scenario_export.h>
#include <vector>

namespace Process
{
class LayerPresenter;
class ProcessFocusManager;
}
namespace score
{
class Document;
} // namespace score

class QAction;
namespace Scenario
{
class ObjectMenuActions;
class ScenarioActions;
class TemporalScenarioPresenter;
class ToolMenuActions;
class ProcessModel;
class ScenarioInterface;
class StateModel;

class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioApplicationPlugin final
    : public QObject,
      public score::GUIApplicationPlugin
{
  Q_OBJECT
  friend class ScenarioContextMenuManager;

public:
  ScenarioApplicationPlugin(const score::GUIApplicationContext& app);
  ~ScenarioApplicationPlugin();

  GUIElements makeGUIElements() override;

  TemporalScenarioPresenter* focusedPresenter() const;

  void reinit_tools();

  Scenario::EditionSettings& editionSettings()
  {
    return m_editionSettings;
  }

  Process::ProcessFocusManager* processFocusManager() const;
  Process::LayerContextMenuManager& layerContextMenuRegistrar()
  {
    return m_layerCtxMenuManager;
  }
  const Process::LayerContextMenuManager& layerContextMenuRegistrar() const
  {
    return m_layerCtxMenuManager;
  }

  Scenario::ScenarioExecution& execution()
  {
    return m_execution;
  }

Q_SIGNALS:
  void keyPressed(int);
  void keyReleased(int);

private:
  void prepareNewDocument() override;
  void on_initDocument(score::Document& doc) override;
  void on_createdDocument(score::Document& doc) override;

  void on_documentChanged(
      score::Document* olddoc, score::Document* newdoc) override;

  void on_activeWindowChanged() override;

  void on_presenterFocused(Process::LayerPresenter* lm);
  void on_presenterDefocused(Process::LayerPresenter* lm);

  QMetaObject::Connection m_focusConnection, m_defocusConnection,
      m_contextMenuConnection;
  Scenario::EditionSettings m_editionSettings;
  Process::LayerContextMenuManager m_layerCtxMenuManager;
  ScenarioExecution m_execution;

  ObjectMenuActions m_objectActions{this};
  ToolMenuActions m_toolActions{this};
  TransportActions m_transportActions{context};
  QAction* m_showCables{};
};
}
