#pragma once
#include <Process/TimeValue.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_engine_export.h>

#include <functional>
#include <memory>
namespace Scenario
{
class IntervalModel;
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;
class ScenarioApplicationPlugin;
class TransportActions;
}
namespace Transport
{
class TransportInterface;
}
namespace Execution
{
using TransportInterface = Transport::TransportInterface;
struct Context;
class Clock;
class BaseScenarioElement;
using exec_setup_fun
    = std::function<void(const Execution::Context&, Execution::BaseScenarioElement&)>;
class SCORE_PLUGIN_ENGINE_EXPORT ExecutionController : public QObject
{
public:
  ExecutionController(const score::GUIApplicationContext& ctx);
  ~ExecutionController();

  TransportInterface& transport() const noexcept;
  void init_transport();

  TimeVal execution_time() const;

  void on_record(::TimeVal t);
  void on_transport(TimeVal t);

  // User requests playback to the transport interface from the local tree
  void request_play_from_localtree(bool);
  void request_play_global_from_localtree(bool);
  void request_transport_from_localtree(TimeVal);
  void request_stop_from_localtree();
  void request_reinitialize_from_localtree();

  // User requests playback to the transport interface from the GUI
  void request_play_from_here(TimeVal t);
  void request_play_global(bool);
  void request_play_local(bool);
  void request_play_interval(
      Scenario::IntervalModel&, exec_setup_fun setup = {},
      ::TimeVal t = ::TimeVal::zero());
  void request_stop_interval(Scenario::IntervalModel&);
  void request_stop();

private:
  // If the transport interface answers: these functions will "press" the Play, etc...
  // buttons programmatically to put them in the right state, and start the playback
  void trigger_play();
  void trigger_pause();
  void trigger_stop();
  void trigger_reinitialize();

  void on_play_global(bool b);
  void on_play_local(bool b);

  void play_interval(
      Scenario::IntervalModel&, exec_setup_fun setup = {},
      ::TimeVal t = ::TimeVal::zero());

  void stop_interval(Scenario::IntervalModel&);
  void ensure_audio_engine();

  void on_play_local(bool, ::TimeVal t);
  void on_pause();
  void on_stop();

  /**
   * @brief Stop execution and resend the start state
   */
  void on_reinitialize();

  void stop_clock();
  void send_end_state();
  void reset_after_stop();
  void reset_edition();

private:
  Scenario::ScenarioDocumentModel* currentScenarioModel();
  Scenario::ScenarioDocumentPresenter* currentScenarioPresenter();
  score::Document* currentDocument() const;
  std::unique_ptr<Execution::Clock> makeClock(const Execution::Context&);

  const score::GUIApplicationContext& context;
  Scenario::ScenarioApplicationPlugin& m_scenario;
  Scenario::TransportActions& m_actions;
  std::unique_ptr<Execution::Clock> m_clock;
  Transport::TransportInterface* m_transport{};

  struct IntervalToPlay
  {
    Scenario::IntervalModel& interval;
    exec_setup_fun setup;
    ::TimeVal t;
  };
  std::vector<IntervalToPlay> m_intervalsToPlay;

  bool m_playing{false};
  bool m_paused{false};
  bool m_requestLocalPlay{};
};
}
