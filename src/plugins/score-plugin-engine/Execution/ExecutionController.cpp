#include "ExecutionController.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/Settings/Model.hpp>
#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/state.hpp>

#include <QMainWindow>

#include <Transport/DocumentPlugin.hpp>
#include <Transport/TransportInterface.hpp>

/**
 * Execution state-machine explanation:
 *
 * * There are multiple sources for what controls the transport:
 * - From the GUI
 * - From the local tree or remote control plug-in
 * - From an external software (e.g. JACK transport)
 * - Automatically (e.g. --autoplay argument on command-line)
 *
 * * No matter the transport source, if there's a GUI its state must be consistent
 *   with what happens.
 *
 * Thus:
 * * The do_*** functions are requests
 * * The trigger_*** functions will perform the action and set the GUI state correctly
 * * The on_*** contain the actual implementation of the transport operation
 *
 * Notes on QAction:
 * * QAction::toggle(); -> will send toggled(true/false);
 * * QAction::trigger(); -> will send toggled(true/false); triggered(true/false);
 * * QAction::check(b); -> will send toggled(b);
 * * GUI: like QAction::trigger
 *
 * Thus, triggered can be used to differentiate between
 * * called from the GUI
 * * called from the software
 *
 * Example of call sequence:
 *
 * * Pressing the "global play button" (stop -> play):
 *   -> QAction::toggled(true)
 *     -> GUI changes (see TransportActions ctor)
 *   -> QAction::triggered(true)
 *     -> ApplicationPlugin::request_play_global(true)
 *     -> TransportInterface::requestPlay()
 *   -> the transport implementation eventually sends the "play()" signal
 *     -> &ApplicationPlugin::trigger_play_global
 *
 * * Pressing the "global play button" (play -> pause):
 *   -> QAction::toggled(false)
 *     -> GUI changes (see TransportActions ctor)
 *   -> QAction::triggered(false)
 *     -> ApplicationPlugin::request_play_global(false)
 *     -> TransportInterface::requestPause()
 *   -> the transport implementation eventually sends the "pause()" signal
 *     -> &ApplicationPlugin::trigger_pause()
 *
 * * Playing with "play from here":
 *   -> ApplicationPlugin::request_play(true, t)
 *   -> TransportInterface::requestTransport(t)
 *   -> TransportInterface::requestPlay
 *   -> The transport implementation eventually sends the "transport()" signal
 *     -> ??
 *   -> The transport implementation eventually sends the "play()" signal
 *     -> &ApplicationPlugin::trigger_play_global
 *
 * * Starting the transport from QJackCtl with a GUI
 *   -> the transport implementation eventually sends the "play()" signal
 *     -> &ApplicationPlugin::trigger_play_global
 *
 * * Starting the transport from QJackCtl without a GUI
 *   -> the transport implementation eventually sends the "play()" signal
 *     -> &ApplicationPlugin::trigger_play_global
 *
 * * Receiving a "/play" message to the local tree with a GUI
 *   -> ApplicationPlugin::request_play
 *   -> TransportInterface::requestPlay
 *   -> The transport implementation eventually sends the "play()" signal
 *     -> &ApplicationPlugin::trigger_play_global
 *
 * * Receiving a "/play" message to the local tree without a GUI
 */

namespace Execution
{
ExecutionController::ExecutionController(const score::ApplicationContext& ctx)
    : context{ctx}
{
}

ExecutionController::ExecutionController(const score::GUIApplicationContext& ctx)
    : context{ctx}
{
}

void ExecutionController::setup_actions()
{
  if(context.applicationSettings.gui)
  {
    using namespace Actions;
    auto& ctx = score::GUIAppContext();
    auto& acts = ctx.actions;

    auto& scenario = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    this->m_actions = &scenario.transportActions();
    connect(
        acts.action<Play>().action(), &QAction::triggered, this,
        &ExecutionController::request_play_local, Qt::QueuedConnection);

    connect(
        acts.action<PlayGlobal>().action(), &QAction::triggered, this,
        &ExecutionController::request_play_global, Qt::QueuedConnection);

    connect(
        acts.action<Stop>().action(), &QAction::triggered, this,
        &ExecutionController::request_stop, Qt::QueuedConnection);

    connect(
        acts.action<Reinitialize>().action(), &QAction::triggered, this,
        &ExecutionController::trigger_reinitialize, Qt::QueuedConnection);

    connect(
        &scenario.execution(), &Scenario::ScenarioExecution::playAtDate, this,
        &ExecutionController::request_play_from_here);
  }
}

ExecutionController::~ExecutionController()
{
  m_transport->teardown();
}

TransportInterface& ExecutionController::transport() const noexcept
{
  return *m_transport;
}

void ExecutionController::request_play_global(bool b)
{
  if(b)
  {
    this->m_requestLocalPlay = false;
    m_transport->requestPlay();
  }
  else
  {
    m_transport->requestPause();
  }
}

void ExecutionController::request_play_local(bool b)
{
  if(b)
  {
    this->m_requestLocalPlay = true;
    m_transport->requestPlay();
  }
  else
  {
    m_transport->requestPause();
  }
}

void ExecutionController::request_play_interval(
    Scenario::IntervalModel& itv, exec_setup_fun setup, TimeVal t)
{
  m_intervalsToPlay.push_back({itv, std::move(setup), t});
  m_transport->requestPlay();
}

void ExecutionController::request_stop_interval(Scenario::IntervalModel& itv)
{
  stop_interval(itv);
}

void ExecutionController::request_stop()
{
  m_transport->requestStop();
}

void ExecutionController::trigger_play()
{
  if(!m_intervalsToPlay.empty())
  {
    if(m_actions)
      m_actions->onPlayLocal();
    for(auto& to_play : m_intervalsToPlay)
      play_interval(to_play.interval, std::move(to_play.setup), to_play.t);
    m_intervalsToPlay.clear();
  }
  else if(this->m_requestLocalPlay)
  {
    if(m_actions)
      m_actions->onPlayLocal();
    on_play_local(true);
  }
  else
  {
    if(m_actions)
      m_actions->onPlayGlobal();
    on_play_global(true);
  }

  // Note: we set this here so that external "play" request are global
  // as it makes more sense:
  // local play is just for checking a sub-part of the score, but the external tools's times will
  // always be relative to the whole general score
  this->m_requestLocalPlay = false;
}

void ExecutionController::trigger_pause()
{
  if(m_actions)
    m_actions->onPause();
  on_pause();
}

void ExecutionController::trigger_stop()
{
  if(m_actions)
    m_actions->onStop();
  on_stop();

  // See note above
  this->m_requestLocalPlay = false;
}

void ExecutionController::trigger_reinitialize()
{
  if(m_actions)
    m_actions->onStop();
  on_reinitialize();

  // See note above
  this->m_requestLocalPlay = false;
}

void ExecutionController::on_play_global(bool b)
{
  if(auto scenar = currentScenarioModel())
  {
    if(b)
    {
      play_interval(scenar->baseInterval(), {}, TimeVal::zero());
    }
    else
    {
      on_pause();
    }
  }
}

void ExecutionController::on_play_local(bool b, ::TimeVal t)
{
  if(auto scenar = currentScenarioPresenter())
  {
    if(b)
    {
      std::optional<TimeVal> startTime;
      auto& itv = scenar->displayedInterval();
      {
        if(itv.startMarker() != TimeVal::zero())
          startTime = itv.startMarker();
      }
      if(!startTime)
        if(t != TimeVal::zero())
          startTime = t;

      if(startTime && m_playing && m_clock)
      {
        if(m_clock->paused())
        {
          on_transport(*startTime);
        }
      }
      play_interval(scenar->displayedInterval(), {}, startTime ? *startTime : t);
    }
    else
    {
      on_pause();
    }
  }
  else
  {
    on_play_global(b);
  }
}

void ExecutionController::on_play_local(bool b)
{
  on_play_local(b, ::TimeVal::zero());
}

void ExecutionController::on_pause()
{
  ensure_audio_engine();

  if(m_clock)
  {
    m_clock->pause();
    m_paused = true;

    if(auto doc = currentDocument())
    {
      doc->context().execTimer.stop();

      if(auto transport_plug = doc->context().findPlugin<Transport::DocumentPlugin>())
        transport_plug->pause();
    }
  }
}

void ExecutionController::on_transport(TimeVal t)
{
  if(!m_clock)
    return;

  SCORE_ASSERT(m_clock->scenario);
  auto itv = m_clock->scenario->baseInterval().OSSIAInterval();
  if(!itv)
    return;

  auto& settings = context.settings<Execution::Settings::Model>();
  auto& ctx = m_clock->context;
  if(settings.getTransportValueCompilation())
  {
    auto execState = m_clock->context.execState;
    ctx.executionQueue.enqueue([execState, itv, time = m_clock->context.time(t)] {
      itv->offset(time);
      execState->commit();
    });
  }
  else
  {
    ctx.executionQueue.enqueue(
        [itv, time = m_clock->context.time(t)] { itv->transport(time); });
  }

  if(auto doc = currentDocument())
    if(auto transport_plug = doc->context().findPlugin<Transport::DocumentPlugin>())
      transport_plug->transport(t);
}

void ExecutionController::request_play_from_localtree(bool val)
{
  if(!m_playing && val)
  {
    // not playing, play requested
    request_play_local(val);
  }
  else if(m_playing)
  {
    if(m_paused == val)
    {
      // paused, play requested
      // or playing, pause requested
      request_play_local(val);
    }
  }
}

void ExecutionController::request_play_global_from_localtree(bool val)
{
  if(!m_playing && val)
  {
    // not playing, play requested
    request_play_global(val);
  }
  else if(m_playing)
  {
    if(m_paused == val)
    {
      // paused, play requested
      // or playing, pause requested
      request_play_global(val);
    }
  }
}

void ExecutionController::request_transport_from_localtree(TimeVal t)
{
  m_transport->requestTransport(t);
}

void ExecutionController::request_stop_from_localtree()
{
  trigger_stop();
}

void ExecutionController::request_reinitialize_from_localtree()
{
  trigger_reinitialize();
}

void ExecutionController::request_play_from_here(TimeVal t)
{
  if(m_clock)
  {
    m_transport->requestTransport(t);
  }
  else
  {
    on_play_local(true, t);

    // FIXME this ends up calling play_interval again...
    if(this->context.applicationSettings.gui)
    {
      auto act = score::GUIAppContext().actions.action<Actions::Play>().action();
      act->trigger();
    }
  }
}

void ExecutionController::ensure_audio_engine()
{
  auto& audio_engine = this->context.applicationPlugin<Audio::ApplicationPlugin>();
  if(!audio_engine.audio)
  {
    if(this->context.applicationSettings.gui)
    {
      score::warning(
          score::GUIAppContext().mainWindow, tr("Cannot play"),
          tr("Cannot start playback. It looks like the audio engine is not "
             "running.\n"
             "Check the audio settings in the software settings to ensure "
             "that a sound card "
             "is correctly configured.\n\n"
             "Check Settings > Audio > Device in particular. "
             "The power-on icon at the bottom of the transport toolbar will "
             "light up when the engine is running."));
      return;
    }
    else
    {
      qFatal("Cannot playback without an audio engine set up");
    }
  }
}

void ExecutionController::play_interval(
    Scenario::IntervalModel& cst, exec_setup_fun setup_fun, TimeVal t)
{
  auto doc = currentDocument();
  if(!doc)
    return;

  auto& ctx = doc->context();

  auto exec_plug = ctx.findPlugin<Execution::DocumentPlugin>();
  if(!exec_plug)
    return;

  ensure_audio_engine();

  if(m_playing)
  {
    SCORE_ASSERT(bool(m_clock));
    if(m_clock->paused())
    {
      m_clock->resume();
      m_paused = false;
    }
    else
    {
      // We are playing an interval while we are already executing.
      if(auto scenar = qobject_cast<Scenario::ProcessModel*>(cst.parent()))
      {
        auto exec_comp = score::findComponent<Execution::ScenarioComponentBase>(
            scenar->components());
        if(exec_comp)
        {
          exec_comp->playInterval(cst);
        }
        return;
      }
    }
  }
  else
  {
    // Here we stop the listening when we start playing the scenario.
    // Get all the selected nodes
    if(auto explorer = Explorer::try_deviceExplorerFromObject(*doc))
    {
      // Disable listening for everything
      if(explorer && !exec_plug->settings.getExecutionListening())
      {
        explorer->deviceModel().listening().stop();
      }

      if(this->context.applicationSettings.gui)
      {
        if(auto w = Explorer::findDeviceExplorerWidgetInstance(score::GUIAppContext()))
        {
          w->setEditable(false);
        }
      }
    }

    exec_plug->reload(cst);

    auto& c = exec_plug->context();
    m_clock = makeClock(c);

    if(setup_fun)
    {
      exec_plug->runAllCommands();
      SCORE_ASSERT(exec_plug->baseScenario());
      setup_fun(c, *exec_plug->baseScenario());
      exec_plug->runAllCommands();
    }

    m_clock->play(t);
    m_paused = false;
  }

  m_playing = true;
  ctx.execTimer.start();

  if(auto transport_plug = ctx.findPlugin<Transport::DocumentPlugin>())
    transport_plug->play();
}

void ExecutionController::stop_interval(Scenario::IntervalModel& cst)
{
  auto doc = currentDocument();
  if(!doc)
    return;

  auto& ctx = doc->context();

  auto exec_plug = ctx.findPlugin<Execution::DocumentPlugin>();
  if(!exec_plug)
    return;

  ensure_audio_engine();

  if(m_playing)
  {
    // We are stopping an interval while we are already executing.
    if(auto scenar = qobject_cast<Scenario::ProcessModel*>(cst.parent()))
    {
      auto exec_comp
          = score::findComponent<Execution::ScenarioComponentBase>(scenar->components());
      if(exec_comp)
      {
        exec_comp->stopInterval(cst);
      }
    }
  }
}

TimeVal ExecutionController::execution_time() const
{
  if(m_clock)
  {
    SCORE_ASSERT(m_clock->scenario);
    auto& itv = m_clock->scenario->baseInterval().scoreInterval().duration;
    return TimeVal(itv.defaultDuration() * itv.playPercentage());
  }
  return TimeVal::zero();
}

void ExecutionController::on_record(::TimeVal t)
{
  SCORE_ASSERT(!m_playing);

  // TODO have a on_exit handler to properly stop the scenario.
  if(auto scenar = currentScenarioModel())
  {
    if(auto exec_plug = scenar->context().findPlugin<Execution::DocumentPlugin>())
    {
      // Listening isn't stopped here.
      exec_plug->reload(scenar->baseInterval());
      m_clock = makeClock(exec_plug->context());
      m_clock->play(t);

      scenar->context().execTimer.start();
      m_playing = true;
      m_paused = false;

      if(auto transport_plug = scenar->context().findPlugin<Transport::DocumentPlugin>())
        transport_plug->play();
    }
  }
}

void ExecutionController::on_stop()
{
  bool wasplaying = m_playing;
  m_playing = false;
  m_paused = false;

  stop_clock();

  if(wasplaying)
    send_end_state();

  reset_after_stop();
}

void ExecutionController::stop_clock()
{
  if(m_clock)
  {
    auto clock = std::move(m_clock);
    m_clock.reset();
    if(auto scenar = currentScenarioModel())
      if(auto exec_plug = scenar->context().findPlugin<Execution::DocumentPlugin>())
      {
        try
        {
          clock->stop();
        }
        catch(...)
        {
          qDebug() << "Error while stopping the clock. There is likely an audio "
                      "hardware issue.";
        }
      }
  }
}

void ExecutionController::send_end_state()
{
  if(auto doc = currentDocument())
  {
    auto& ctx = doc->context();
    if(auto exec_plug = ctx.findPlugin<Execution::DocumentPlugin>())
    {
      exec_plug->playStopState();
    }
  }
}
void ExecutionController::reset_after_stop()
{
  if(auto doc = currentDocument())
  {
    auto& ctx = doc->context();
    ctx.execTimer.stop();

    if(auto transport_plug = ctx.findPlugin<Transport::DocumentPlugin>())
      transport_plug->stop();

    if(auto exec_plug = ctx.findPlugin<Execution::DocumentPlugin>())
    {
      exec_plug->clear();
    }
    else
    {
      return;
    }

    // If we can we resume listening
    if(context.applicationSettings.gui)
    {
      auto& guictx = score::GUIAppContext();
      if(!guictx.docManager.preparingNewDocument())
      {
        auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
        if(explorer)
          explorer->deviceModel().listening().restore();
      }
    }
  }

  auto scenar = currentScenarioModel();
  if(!scenar || scenar->closing())
    return;
  QTimer::singleShot(
      50, this,
      [this, itv = QPointer<Scenario::IntervalModel>{&scenar->baseInterval()}] {
    if(itv)
      reset_edition();
      });
}

void ExecutionController::reset_edition()
{
  // Reset edition for the device explorer
  if(context.applicationSettings.gui)
  {
    auto& gctx = score::GUIAppContext();
    if(auto w = Explorer::findDeviceExplorerWidgetInstance(gctx))
    {
      w->setEditable(true);
    }
  }

  // FIXME uuuugh have an event in scenario instead (or better, move this
  // in process)
  auto scenar = currentScenarioModel();
  if(!scenar)
    return;
  scenar->baseInterval().reset();
  scenar->baseInterval().executionEvent(Scenario::IntervalExecutionEvent::Finished);
}

void ExecutionController::on_reinitialize()
{
  // TODO to be more precise, we should stop the execution, but keep
  // the execution_state alive until the last message is sent
  if(auto scenar = currentScenarioModel())
  {
    auto& ctx = scenar->context();
    auto& doc = ctx.document;
    auto exec_plug = ctx.findPlugin<Execution::DocumentPlugin>();
    if(!exec_plug)
      return;

    if(context.applicationSettings.gui)
    {
      auto& gctx = score::GUIAppContext();
      auto explorer = Explorer::try_deviceExplorerFromObject(doc);

      // Disable listening for everything
      if(explorer)
        if(!ctx.app.settings<Execution::Settings::Model>().getExecutionListening())
          explorer->deviceModel().listening().stop();

      // If we can we resume listening
      if(explorer)
        if(!gctx.docManager.preparingNewDocument())
          explorer->deviceModel().listening().restore();
    }

    on_stop();

    exec_plug->playStartState();
  }
}

void ExecutionController::init_transport()
{
  if(m_transport)
    m_transport->teardown();

  auto& s = context.settings<Execution::Settings::Model>();
  m_transport = s.getTransport();
  SCORE_ASSERT(m_transport);
  m_transport->setup();
  connect(
      m_transport, &Execution::TransportInterface::play, this,
      &ExecutionController::trigger_play);
  connect(
      m_transport, &Execution::TransportInterface::pause, this,
      &ExecutionController::trigger_pause);
  connect(
      m_transport, &Execution::TransportInterface::stop, this,
      &ExecutionController::trigger_stop);
  connect(
      m_transport, &Execution::TransportInterface::transport, this,
      &ExecutionController::on_transport);

  auto& audio_settings = this->context.settings<Audio::Settings::Model>();
  con(audio_settings, &Audio::Settings::Model::JackTransportChanged, this,
      &ExecutionController::init_transport, Qt::UniqueConnection);
}

Scenario::ScenarioDocumentModel* ExecutionController::currentScenarioModel()
{
  if(auto doc = currentDocument())
  {
    return score::IDocument::try_modelDelegate<Scenario::ScenarioDocumentModel>(*doc);
  }
  return nullptr;
}

Scenario::ScenarioDocumentPresenter* ExecutionController::currentScenarioPresenter()
{
  if(auto doc = currentDocument())
  {
    return score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
        *doc);
  }
  return nullptr;
}

score::Document* ExecutionController::currentDocument() const
{
  return context.documents.currentDocument();
}

std::unique_ptr<Execution::Clock>
ExecutionController::makeClock(const Execution::Context& ctx)
{
  auto& s = context.settings<Execution::Settings::Model>();
  auto clk = s.makeClock(ctx);
  SCORE_ASSERT(clk->scenario);
  auto& itv = clk->scenario->baseInterval().interval();
  con(itv, &IdentifiedObjectAbstract::identified_object_destroying, this,
      [this] { trigger_stop(); });
  return clk;
}

}
