#pragma once

#include <Process/TimeValue.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <Execution/ContextMenu/PlayContextMenu.hpp>
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
using exec_setup_fun
    = std::function<void(const Execution::Context&, Execution::BaseScenarioElement&)>;
class SCORE_PLUGIN_ENGINE_EXPORT ApplicationPlugin final : public QObject,
                                                           public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  bool handleStartup() override;
  score::GUIElements makeGUIElements() override;

  void on_initDocument(score::Document& doc) override;
  void on_createdDocument(score::Document& doc) override;

  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;

  void prepareNewDocument() override;

  void on_play(bool, ::TimeVal t = ::TimeVal::zero());
  void on_play(
      Scenario::IntervalModel&,
      bool,
      exec_setup_fun setup = {},
      ::TimeVal t = ::TimeVal::zero());

  void on_record(::TimeVal t);

  bool playing() const { return m_playing; }

  bool paused() const { return m_paused; }

  void on_stop();

private:
  void on_init();
  void initialize() override;
  void on_transport(TimeVal t);
  void initLocalTreeNodes(LocalTree::DocumentPlugin&);
  QWidget* setupTimingWidget(QLabel*) const;

  std::unique_ptr<Execution::Clock> makeClock(const Execution::Context&);

  Execution::PlayContextMenu m_playActions;

  std::unique_ptr<Execution::Clock> m_clock;
  Scenario::SpeedWidget* m_speedSlider{};
  bool m_playing{false}, m_paused{false};
};
}
