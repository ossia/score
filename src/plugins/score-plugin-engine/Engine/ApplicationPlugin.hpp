#pragma once

#include <Process/TimeValue.hpp>

#include <Execution/ContextMenu/PlayContextMenu.hpp>
#include <Execution/ExecutionController.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <score_plugin_engine_export.h>

#include <memory>
namespace Scenario
{
class SpeedWidget;
}
namespace Scenario
{
class IntervalModel;
}
namespace Execution
{
struct Context;
class Clock;
class BaseScenarioElement;
}

namespace LocalTree
{
class DocumentPlugin;
}

namespace ossia
{
class audio_engine;
}

class QLabel;
namespace Engine
{

class SCORE_PLUGIN_ENGINE_EXPORT ApplicationPlugin final
    : public QObject
    , public score::ApplicationPlugin
{
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  ~ApplicationPlugin() override;

  void initialize() override;

  bool handleStartup() override;

  void prepareNewDocument() override;
  void on_initDocument(score::Document& doc) override;
  void on_createdDocument(score::Document& doc) override;

  Execution::ExecutionController& execution() { return m_execution; }

private:
  Execution::ExecutionController m_execution;
};

class SCORE_PLUGIN_ENGINE_EXPORT GUIApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  ~GUIApplicationPlugin() override;

  Execution::ExecutionController& execution() { return m_execution; }

  void initialize() override;
  score::GUIElements makeGUIElements() override;
  void on_createdDocument(score::Document& doc) override;
  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;
  void initLocalTreeNodes(LocalTree::DocumentPlugin&);

  QWidget* setupTimingWidget(QLabel*) const;

private:
  Execution::ExecutionController& m_execution;
  Scenario::SpeedWidget* m_speedSlider{};
  QAction* m_musicalAct{};
  Execution::PlayContextMenu m_playActions;
};
}
