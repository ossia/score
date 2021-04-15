#pragma once
#include <Process/TimeValue.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <memory>
#include <functional>
#include <score_plugin_engine_export.h>
namespace Scenario
{
class IntervalModel;
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;
class ScenarioApplicationPlugin;
class TransportActions;
}

namespace Execution
{
struct Context;
class Clock;
class TransportInterface;
class BaseScenarioElement;
using exec_setup_fun
    = std::function<void(const Execution::Context&, Execution::BaseScenarioElement&)>;
class SCORE_PLUGIN_ENGINE_EXPORT ExecutionManager : public QObject
{
public:
  ExecutionManager(const score::GUIApplicationContext& ctx);
  ~ExecutionManager();

  TimeVal execution_time() const;
  bool playing() const { return m_playing; }
  bool paused() const { return m_paused; }

  void on_record(::TimeVal t);
  void on_transport(TimeVal t);

  void request_play_from_localtree(bool);
  void request_play_global_from_localtree(bool);
  void request_transport_from_localtree(TimeVal);
  void request_stop_from_localtree();
  void request_reinitialize_from_localtree();

  // User requests playback to the transport interface from the GUI
  void request_play_from_here(TimeVal t);
  void request_play_global(bool);
  void request_play_local(bool);
  void request_stop();

  // If the transport interface answers: these functions will "press" the Play, etc...
  // buttons programmatically to put them in the right state, OR
  // start playback directly if there's no GUI
  void trigger_play_global();
  void trigger_play_local();
  void trigger_pause();
  void trigger_stop();
  void trigger_reinitialize();

  void on_play_global(bool b);
  void on_play_local(bool b);

  void play_interval(
      Scenario::IntervalModel&,
      exec_setup_fun setup = {},
      ::TimeVal t = ::TimeVal::zero());

  void init_transport();
  void ensure_audio_engine();

  void on_play_local(bool, ::TimeVal t);
  void on_pause();
  void on_stop();

  /**
   * @brief Stop execution and resend the start state
   */
  void on_reinitialize();

private:

  Scenario::ScenarioDocumentModel* currentScenarioModel();
  Scenario::ScenarioDocumentPresenter* currentScenarioPresenter();
  score::Document* currentDocument() const;
  std::unique_ptr<Execution::Clock> makeClock(const Execution::Context&);

  const score::GUIApplicationContext& context;
  Scenario::ScenarioApplicationPlugin& m_scenario;
  Scenario::TransportActions& m_actions;
  std::unique_ptr<Execution::Clock> m_clock;
  Execution::TransportInterface* m_transport{};
  bool m_playing{false};
  bool m_paused{false};
};
}
