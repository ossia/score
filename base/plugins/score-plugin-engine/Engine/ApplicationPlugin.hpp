#pragma once

#include <Engine/Executor/ContextMenu/PlayContextMenu.hpp>
#include <Process/TimeValue.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score_plugin_engine_export.h>
#include <QString>
#include <memory>

namespace Scenario
{ class IntervalModel; }
namespace Engine::Execution
{
struct Context;
class ClockManager;
}

namespace Engine::LocalTree
{
class DocumentPlugin;
}

namespace ossia
{ class audio_engine; }

namespace Engine
{
class SCORE_PLUGIN_ENGINE_EXPORT ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  bool handleStartup() override;
  score::GUIElements makeGUIElements() override;

  void on_initDocument(score::Document& doc) override;
  void on_createdDocument(score::Document& doc) override;

  void on_documentChanged(
      score::Document* olddoc, score::Document* newdoc) override;

  void prepareNewDocument() override;

  void on_play(bool, ::TimeVal t = ::TimeVal::zero());
  void on_play(
      Scenario::IntervalModel&,
      bool,
      std::function<void(const Engine::Execution::Context&)> setup = {},
      ::TimeVal t = ::TimeVal::zero());

  void on_record(::TimeVal t);

  bool playing() const
  {
    return m_playing;
  }

  bool paused() const
  {
    return m_paused;
  }

  void on_stop();

  std::unique_ptr<ossia::audio_engine> audio;

private:
  void on_init();
  void initialize() override;
  void on_transport(TimeVal t);
  void initLocalTreeNodes(Engine::LocalTree::DocumentPlugin&);

  std::unique_ptr<Engine::Execution::ClockManager>
  makeClock(const Engine::Execution::Context&);

  Engine::Execution::PlayContextMenu m_playActions;

  std::unique_ptr<Engine::Execution::ClockManager> m_clock;
  QAction* m_audioEngineAct{};
  bool m_playing{false}, m_paused{false};

  bool m_updating_audio = false;
  void restart_engine();
  void setup_engine();
};
}
