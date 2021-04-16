#include "ExecutionController.hpp"
#include <Execution/Transport/TransportInterface.hpp>
#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <Audio/AudioApplicationPlugin.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <QMainWindow>

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
ExecutionController::ExecutionController(const score::GUIApplicationContext& ctx)
  : context{ctx}
  , m_scenario{ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()}
  , m_actions{m_scenario.transportActions()}
{
  if (ctx.applicationSettings.gui)
  {
    auto& acts = ctx.actions;
    using namespace Actions;
    connect(acts.action<Play>().action(), &QAction::triggered,
            this, &ExecutionController::request_play_local,
            Qt::QueuedConnection);

    connect(acts.action<PlayGlobal>().action(), &QAction::triggered,
            this, &ExecutionController::request_play_global,
            Qt::QueuedConnection);

    connect(acts.action<Stop>().action(), &QAction::triggered,
            this, &ExecutionController::request_stop,
            Qt::QueuedConnection);

    connect(acts.action<Reinitialize>().action(), &QAction::triggered,
            this, &ExecutionController::on_reinitialize,
            Qt::QueuedConnection);

    connect(&m_scenario.execution(), &Scenario::ScenarioExecution::playAtDate,
            this, &ExecutionController::request_play_from_here);
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
    Scenario::IntervalModel& itv,
    exec_setup_fun setup,
    TimeVal t)
{
  m_intervalsToPlay.push_back({itv, std::move(setup), t});
  m_transport->requestPlay();
}

void ExecutionController::request_stop()
{
  m_transport->requestStop();
}


void ExecutionController::trigger_play()
{
  if(!m_intervalsToPlay.empty())
  {
    m_actions.onPlayLocal();
    for(auto& to_play : m_intervalsToPlay)
      play_interval(to_play.interval, std::move(to_play.setup), to_play.t);
    m_intervalsToPlay.clear();
  }
  else if(this->m_requestLocalPlay)
  {
    m_actions.onPlayLocal();
    on_play_local(true);
  }
  else
  {
    m_actions.onPlayGlobal();
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
  m_actions.onPause();
  on_pause();
}

void ExecutionController::trigger_stop()
{
  m_actions.onStop();
  on_stop();

  // See note above
  this->m_requestLocalPlay = false;
}

void ExecutionController::trigger_reinitialize()
{
  m_actions.onStop();
  on_reinitialize();

  // See note above
  this->m_requestLocalPlay = false;
}


void ExecutionController::on_play_global(bool b)
{
  if (auto scenar = currentScenarioModel())
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
  if (auto scenar = currentScenarioPresenter())
  {
    if(b)
    {
      play_interval(scenar->displayedInterval(), {}, t);
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

  if (m_clock)
  {
    m_clock->pause();
    m_paused = true;

    if (auto doc = currentDocument())
      doc->context().execTimer.stop();
  }
}


void ExecutionController::on_transport(TimeVal t)
{
  if (!m_clock)
    return;

  auto itv = m_clock->scenario.baseInterval().OSSIAInterval();
  if (!itv)
    return;

  auto& settings = context.settings<Execution::Settings::Model>();
  auto& ctx = m_clock->context;
  if (settings.getTransportValueCompilation())
  {
    auto execState = m_clock->context.execState;
    ctx.executionQueue.enqueue([execState, itv, time = m_clock->context.time(t)] {
      itv->offset(time);
      execState->commit();
    });
  }
  else
  {
    ctx.executionQueue.enqueue([itv, time = m_clock->context.time(t)] { itv->transport(time); });
  }
}

void ExecutionController::request_play_from_localtree(bool val)
{
  if (!m_playing && val)
  {
    // not playing, play requested
    request_play_local(val);
  }
  else if (m_playing)
  {
    if (m_paused == val)
    {
      // paused, play requested
      // or playing, pause requested
      request_play_local(val);
    }
  }
}

void ExecutionController::request_play_global_from_localtree(bool val)
{
  if (!m_playing && val)
  {
    // not playing, play requested
    request_play_global(val);
  }
  else if (m_playing)
  {
    if (m_paused == val)
    {
      // paused, play requested
      // or playing, pause requested
      request_play_global(val);
    }
  }
}

void ExecutionController::request_transport_from_localtree(TimeVal t)
{
  on_transport(t);
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
  if (m_clock)
  {
    on_transport(t);
  }
  else
  {
    on_play_local(true, t);
    auto act = this->context.actions.action<Actions::Play>().action();
    act->trigger();
  }
}

void ExecutionController::ensure_audio_engine()
{
  auto& audio_engine = this->context.guiApplicationPlugin<Audio::ApplicationPlugin>();
  if (!audio_engine.audio)
  {
    if (this->context.mainWindow)
    {
      score::warning(
          this->context.mainWindow,
          tr("Cannot play"),
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
    Scenario::IntervalModel& cst,
    exec_setup_fun setup_fun,
    TimeVal t)
{
  auto doc = currentDocument();
  if (!doc)
    return;

  auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();
  if (!plugmodel)
    return;

  ensure_audio_engine();

  if (m_playing)
  {
    SCORE_ASSERT(bool(m_clock));
    if (m_clock->paused())
    {
      m_clock->resume();
      m_paused = false;
    }
  }
  else
  {
    // Here we stop the listening when we start playing the scenario.
    // Get all the selected nodes
    if(auto explorer = Explorer::try_deviceExplorerFromObject(*doc))
    {
      // Disable listening for everything
      if (explorer && !plugmodel->settings.getExecutionListening())
      {
        explorer->deviceModel().listening().stop();
      }

      if (this->context.applicationSettings.gui)
      {
        if (auto w = Explorer::findDeviceExplorerWidgetInstance(this->context))
        {
          w->setEditable(false);
        }
      }
    }

    plugmodel->reload(cst);

    auto& c = plugmodel->context();
    m_clock = makeClock(c);

    if (setup_fun)
    {
      plugmodel->runAllCommands();
      setup_fun(c, plugmodel->baseScenario());
      plugmodel->runAllCommands();
    }

    m_clock->play(t);
    m_paused = false;
  }

  m_playing = true;
  doc->context().execTimer.start();
}

TimeVal ExecutionController::execution_time() const
{
  if(m_clock)
  {
    auto& itv = m_clock->scenario.baseInterval().scoreInterval().duration;
    return TimeVal(itv.defaultDuration() * itv.playPercentage());
  }
  return TimeVal::zero();
}

void ExecutionController::on_record(::TimeVal t)
{
  SCORE_ASSERT(!m_playing);

  // TODO have a on_exit handler to properly stop the scenario.
  if (auto scenar = currentScenarioModel())
  {
    auto plugmodel = scenar->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;

    // Listening isn't stopped here.
    plugmodel->reload(scenar->baseInterval());
    m_clock = makeClock(plugmodel->context());
    m_clock->play(t);

    scenar->context().execTimer.start();
    m_playing = true;
    m_paused = false;
  }
}

void ExecutionController::on_stop()
{
  bool wasplaying = m_playing;
  m_playing = false;
  m_paused = false;

  // Send the end state
  auto doc = currentDocument();
  if(doc)
  {
    auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();

    if (plugmodel)
    {
      if (wasplaying)
      {
        // TODO this is sent *after* init has been sent, we have to reverse it in on_init
        // but before the thing is cleaned.
        if (auto scenar = currentScenarioModel())
        {
          auto state = Engine::score_to_ossia::state(
              scenar->baseScenario().endState(), plugmodel->context());
          state.launch();
        }
      }
    }
  }

  if (m_clock)
  {
    auto clock = std::move(m_clock);
    m_clock.reset();
    try {
      clock->stop();
    }  catch (...) {
      qDebug() << "Error while stopping the clock. There is likely an audio hardware issue.";
    }
  }

  if (context.applicationSettings.gui)
  {
    if (auto w = Explorer::findDeviceExplorerWidgetInstance(context))
    {
      w->setEditable(true);
    }
  }

  if (doc)
  {
    doc->context().execTimer.stop();

    auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (plugmodel)
    {
      plugmodel->clear();
    }
    else
    {
      return;
    }

    // If we can we resume listening
    if (!context.docManager.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }

    QTimer::singleShot(50, this, &ExecutionController::reset_edition);
  }
}

void ExecutionController::reset_edition()
{
  // FIXME uuuugh have an event in scenario instead (or better, move this
  // in process)
  auto scenar = currentScenarioModel();
  if (!scenar)
    return;
  scenar->baseInterval().reset();
  scenar->baseInterval().executionEvent(Scenario::IntervalExecutionEvent::Finished);
  auto procs = scenar->context().document.findChildren<Scenario::ProcessModel*>();
  for (Scenario::ProcessModel* e : procs)
  {
    for (auto& itv : e->intervals)
    {
      itv.reset();
      itv.executionEvent(Scenario::IntervalExecutionEvent::Finished);
    }
    for (auto& ts : e->timeSyncs)
    {
      ts.setWaiting(false);
    }
    for (auto& ev : e->events)
    {
      ev.setStatus(Scenario::ExecutionStatus::Editing, *e);
    }
  }
}

void ExecutionController::on_reinitialize()
{
  // TODO to be more precise, we should stop the execution, but keep
  // the execution_state alive until the last message is sent
  if (auto scenar = currentScenarioModel())
  {
    auto& ctx = scenar->context();
    auto& doc = ctx.document;
    auto plugmodel = ctx.findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;

    auto explorer = Explorer::try_deviceExplorerFromObject(doc);

    // Disable listening for everything
    if (explorer)
      if(!ctx.app.settings<Execution::Settings::Model>().getExecutionListening())
        explorer->deviceModel().listening().stop();

    plugmodel->playStartState();

    // If we can we resume listening
    if (explorer)
      if (!context.docManager.preparingNewDocument())
        explorer->deviceModel().listening().restore();

    trigger_stop();
  }
}


void ExecutionController::init_transport()
{
  if(m_transport)
    m_transport->teardown();
  delete m_transport;

  auto& s = context.settings<Execution::Settings::Model>();
  m_transport = s.getTransport();
  SCORE_ASSERT(m_transport);
  m_transport->setup();
  connect(m_transport, &Execution::TransportInterface::play,
          this, &ExecutionController::trigger_play);
  connect(m_transport, &Execution::TransportInterface::pause,
          this, &ExecutionController::trigger_pause);
  connect(m_transport, &Execution::TransportInterface::stop,
          this, &ExecutionController::trigger_stop);


  auto& audio_settings = this->context.settings<Audio::Settings::Model>();
  con(audio_settings, &Audio::Settings::Model::JackTransportChanged,
      this, &ExecutionController::init_transport, Qt::UniqueConnection);
}

Scenario::ScenarioDocumentModel* ExecutionController::currentScenarioModel()
{
  if (auto doc = currentDocument())
  {
    return score::IDocument::try_modelDelegate<Scenario::ScenarioDocumentModel>(*doc);
  }
  return nullptr;
}

Scenario::ScenarioDocumentPresenter* ExecutionController::currentScenarioPresenter()
{
  if (auto doc = currentDocument())
  {
    return score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
  }
  return nullptr;
}

score::Document* ExecutionController::currentDocument() const
{
  return context.documents.currentDocument();
}

std::unique_ptr<Execution::Clock> ExecutionController::makeClock(const Execution::Context& ctx)
{
  auto& s = context.settings<Execution::Settings::Model>();
  return s.makeClock(ctx);
}

}

