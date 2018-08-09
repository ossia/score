//// This is an open source non-commercial project. Dear PVS-Studio, please check
//// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationPlugin.hpp"

#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ContextMenu/PlayContextMenu.hpp>
#include <Process/ExecutionContext.hpp>
#include <Execution/Settings/ExecutorModel.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Protocols/Audio/AudioDevice.hpp>
#include <Audio/Settings/Model.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Settings/ExplorerModel.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/TimeValue.hpp>
#include <QAction>
#include <QApplication>
#include <QVariant>
#include <QVector>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <algorithm>
#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <ossia-qt/invoke.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/Todo.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SetIcons.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <vector>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>

#include <QLabel>
#include <wobjectimpl.h>
SCORE_DECLARE_ACTION(
    RestartAudio, "Restart Audio", Common, QKeySequence::UnknownKey)

namespace Engine
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}, m_playActions{*this, ctx}
{
  // qInstallMessageHandler(nullptr);
  // Two parts :
  // One that maintains the devices for each document
  // (and disconnects / reconnects them when the current document changes)
  // Also during execution, one shouldn't be able to switch document.

  // Another part that, at execution time, creates structures corresponding
  // to the Scenario plug-in with the OSSIA API.

  if (ctx.applicationSettings.gui)
  {
    auto& play_action = ctx.actions.action<Actions::Play>();
    connect(
        play_action.action(), &QAction::triggered, this,
        [&](bool b) { on_play(b); }, Qt::QueuedConnection);

    auto& play_glob_action = ctx.actions.action<Actions::PlayGlobal>();
    connect(
        play_glob_action.action(), &QAction::triggered, this,
        [&](bool b) {
          if (auto doc = currentDocument())
          {
            auto& mod = doc->model().modelDelegate();
            auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
            if (scenar)
            {
              on_play(scenar->baseInterval(), b, {}, TimeVal::zero());
            }
          }
        },
        Qt::QueuedConnection);

    auto& stop_action = ctx.actions.action<Actions::Stop>();
    connect(
        stop_action.action(), &QAction::triggered, this,
        &ApplicationPlugin::on_stop, Qt::QueuedConnection);

    auto& init_action = ctx.actions.action<Actions::Reinitialize>();
    connect(
        init_action.action(), &QAction::triggered, this,
        &ApplicationPlugin::on_init, Qt::QueuedConnection);

    auto& ctrl
        = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    con(ctrl.execution(), &Scenario::ScenarioExecution::playAtDate, this,
        [=, act = play_action.action()](const TimeVal& t) {
          if (m_clock)
          {
            on_transport(t);
          }
          else
          {
            on_play(true, t);
            act->trigger();
          }
        });

    m_playActions.setupContextMenu(ctrl.layerContextMenuRegistrar());
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
  // The scenarios playing should already have been stopped by virtue of
  // aboutToClose.
}

bool ApplicationPlugin::handleStartup()
{
  if (!context.documents.documents().empty())
  {
    if (context.applicationSettings.autoplay)
    {
      // TODO what happens if we load multiple documents ?
      on_play(true);
      return true;
    }
  }

  return false;
}

struct AlteredAudioSettings
{
    ossia::optional<QString> driver;
    ossia::optional<QString> card_in;
    ossia::optional<QString> card_out;
    ossia::optional<int> default_ins{};
    ossia::optional<int> default_outs{};
    ossia::optional<int> rate{};
    ossia::optional<int> buffer_size{};
};
static std::unique_ptr<ossia::audio_engine> make_engine(AlteredAudioSettings& alt_set)
{
  auto& set = score::AppContext().settings<Audio::Settings::Model>();
  auto driver = set.getDriver();
  auto req_in = set.getCardIn();
  auto req_out = set.getCardOut();
  auto ins = set.getDefaultIn();
  auto old_ins = ins;
  auto outs = set.getDefaultOut();
  auto old_outs = outs;
  auto rate = set.getRate();
  auto old_rate = rate;
  auto bs = set.getBufferSize();
  auto old_bs = bs;

  ossia::audio_engine* eng{};

  try
  {
    eng = ossia::make_audio_engine(
             driver.toStdString(), "score",
             req_in.toStdString(), req_out.toStdString(),
             ins, outs, rate, bs);
  }
  catch(...)
  {
    try {
      eng = ossia::make_audio_engine(
               "PortAudio", "score",
               req_in.toStdString(), req_out.toStdString(),
               ins, outs, rate, bs);
      alt_set.driver = QString{"PortAudio"};
    }
    catch(...)
    {
      try
      {
        eng = ossia::make_audio_engine(
                 "SDL", "score",
                 req_in.toStdString(), req_out.toStdString(),
                 ins, outs, rate, bs);
        alt_set.driver = QString{"SDL"};
      }
      catch(...)
      {
        eng = ossia::make_audio_engine(
                 "Dummy", "score",
                 req_in.toStdString(), req_out.toStdString(),
                 ins, outs, rate, bs);
        alt_set.driver = QString{"Dummy"};
      }
    }
  }

  if (ins != old_ins)
    alt_set.default_ins = ins;
  if (outs != old_outs)
    alt_set.default_outs = outs;
  if (bs != old_bs)
    alt_set.buffer_size = bs;
  if (rate != old_rate)
    alt_set.rate = rate;

  return std::unique_ptr<ossia::audio_engine>{eng};
}

void ApplicationPlugin::restart_engine()
{
  if(m_updating_audio)
    return;

  if (auto doc = this->currentDocument())
  {
    auto dev = doc->context()
                   .plugin<Explorer::DeviceDocumentPlugin>()
                   .list()
                   .audioDevice();
    if (!dev)
      return;
    auto& d = *dynamic_cast<Dataflow::AudioDevice*>(dev);
    if (audio)
      audio->stop();

    setup_engine();
    d.reconnect();
  }
}

void ApplicationPlugin::setup_engine()
{
  audio.reset();
  AlteredAudioSettings alt;
  audio = make_engine(alt);
  {
    auto& set = score::AppContext().settings<Audio::Settings::Model>();

    m_updating_audio = true;

    if(alt.driver)
      set.setDriver(*alt.driver);
    if(alt.card_in)
      set.setCardIn(*alt.card_in);
    if(alt.card_out)
      set.setCardOut(*alt.card_out);
    if(alt.buffer_size)
      set.setBufferSize(*alt.buffer_size);
    if(alt.rate)
      set.setRate(*alt.rate);
    if(alt.default_ins)
      set.setDefaultIn(*alt.default_ins);
    if(alt.default_outs)
      set.setDefaultOut(*alt.default_outs);

    m_updating_audio = false;
  }
  if(m_audioEngineAct)
    m_audioEngineAct->setChecked(bool(audio));
}

void ApplicationPlugin::initialize()
{
  auto& set = context.settings<Audio::Settings::Model>();

  con(set, &Audio::Settings::Model::DriverChanged, this, &ApplicationPlugin::restart_engine);
  con(set, &Audio::Settings::Model::BufferSizeChanged, this, &ApplicationPlugin::restart_engine);
  con(set, &Audio::Settings::Model::RateChanged, this, &ApplicationPlugin::restart_engine);
  con(set, &Audio::Settings::Model::CardInChanged, this, &ApplicationPlugin::restart_engine);
  con(set, &Audio::Settings::Model::CardOutChanged, this, &ApplicationPlugin::restart_engine);
  con(set, &Audio::Settings::Model::DefaultInChanged, this, &ApplicationPlugin::restart_engine);
  con(set, &Audio::Settings::Model::DefaultOutChanged, this, &ApplicationPlugin::restart_engine);

  try {
  setup_engine();
  } catch(...) {

  }
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto& toolbars = e.toolbars;

  toolbars.reserve(2);

  // The toolbar with the time
  {
    auto bar = new QToolBar;
    auto time_label = new QLabel;
    QFont time_font("Ubuntu", 18, QFont::Weight::DemiBold);
    time_label->setFont(time_font);
    time_label->setText("00:00:00.000");
    bar->addWidget(time_label);
    auto timer = new QTimer{this};
    connect(timer, &QTimer::timeout,
            this, [=] {
      if(m_clock)
      {
        auto& itv = m_clock->scenario.baseInterval().scoreInterval().duration;
        auto time = (itv.defaultDuration() * itv.playPercentage()).toQTime();
        time_label->setText(time.toString("HH:mm:ss.zzz"));
      }
      else
      {
        time_label->setText("00:00:00.000");
      }
    });
    timer->start(1000 / 20);
    toolbars.emplace_back(bar, StringKey<score::Toolbar>("Timing"), Qt::BottomToolBarArea, 100);
  }

  // The toolbar with the speed
  {
    m_speedToolbar = new QToolBar;
    toolbars.emplace_back(m_speedToolbar, StringKey<score::Toolbar>("Speed"), Qt::BottomToolBarArea, 300);
  }

  // The toolbar with the volume control
  m_audioEngineAct = new QAction{tr("Restart Audio"), this};
  m_audioEngineAct->setCheckable(true);
  m_audioEngineAct->setChecked(bool(audio));

  setIcons(
      m_audioEngineAct, QStringLiteral(":/icons/engine_on.png"),
      QStringLiteral(":/icons/engine_off.png"), QStringLiteral(":/icons/engine_disabled.png"), false);
  {
    auto bar = new QToolBar;
    bar->addAction(m_audioEngineAct);
    auto sl = new Control::VolumeSlider{bar};
    sl->setValue(0.5);
    bar->addWidget(sl);
    connect(sl, &Control::VolumeSlider::valueChanged, this, [=] (double v) {
      if (m_clock)
      {
        if (auto& st = m_clock->context.execState)
        {
          for (auto& dev : st->edit_devices())
          {
            if(auto protocol = dynamic_cast<ossia::audio_protocol*>(&dev->get_protocol()))
            {
              auto root
                  = ossia::net::find_node(dev->get_root_node(), "/out/main");
              if (root)
              {
                if (auto p = root->get_parameter())
                {
                  auto audio_p = static_cast<ossia::audio_parameter*>(p);
                  audio_p->push_value(v);
                }
              }
            }
          }
        }
      }
    });

    toolbars.emplace_back(bar, StringKey<score::Toolbar>("Audio"), Qt::BottomToolBarArea, 400);
  }

  e.actions.container.reserve(2);
  e.actions.add<Actions::RestartAudio>(m_audioEngineAct);

  connect(m_audioEngineAct, &QAction::triggered, this, [=] (bool) {
    if (audio)
    {
      audio->stop();
      audio.reset();
    }
    else
    {
      setup_engine();
    }
    m_audioEngineAct->setChecked(bool(audio));

    if (auto doc = currentDocument())
    {
      auto dev = doc->context()
                     .plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .audioDevice();
      if (!dev)
        return;
      auto& d = *static_cast<Dataflow::AudioDevice*>(dev);
      d.reconnect();
    }
  });


  return e;
}


void ApplicationPlugin::on_initDocument(score::Document& doc)
{
#if !defined(__EMSCRIPTEN__)
  score::addDocumentPlugin<LocalTree::DocumentPlugin>(doc);
#endif
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  LocalTree::DocumentPlugin* lt
      = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if (lt)
  {
    lt->init();
    initLocalTreeNodes(*lt);
  }
  score::addDocumentPlugin<Execution::DocumentPlugin>(doc);
}



void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
  if (context.applicationSettings.gui)
  {
    auto cld = m_speedToolbar->findChildren<Scenario::SpeedWidget*>("SpeedSlider");
    if(!cld.empty())
      cld[0]->deleteLater();
  }

  if (olddoc)
  {
    // Disable the local tree for this document by removing
    // the node temporarily
    /*
    auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(false);
    */
  }

  if (newdoc)
  {
    // Enable the local tree for this document.

    /*
    auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(true);
    */

    // Setup speed toolbar
    auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(*newdoc);

    if (context.applicationSettings.gui)
    {
      auto slider = new Scenario::SpeedWidget{root.baseInterval(), newdoc->context(), false, m_speedToolbar};
      m_speedToolbar->addWidget(slider);
    }

    // Setup audio & devices
    auto& doc_plugin
        = newdoc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* set
        = newdoc->context().findPlugin<Explorer::ProjectSettings::Model>();
    if (set)
    {
      if (set->getReconnectOnStart())
      {
        auto& list = doc_plugin.list();
        list.apply([&](Device::DeviceInterface& dev) {
          if (&dev != list.audioDevice() && &dev != list.localDevice())
            dev.reconnect();
        });

        if (set->getRefreshOnStart())
        {
          list.apply([&](Device::DeviceInterface& dev) {
            if (&dev != list.audioDevice() && &dev != list.localDevice())
              if (dev.connected())
              {
                auto old_name = dev.name();
                auto new_node = dev.refresh();

                auto& explorer = doc_plugin.explorer();
                const auto& cld = explorer.rootNode().children();
                for (auto it = cld.begin(); it != cld.end(); ++it)
                {
                  auto ds = it->get<Device::DeviceSettings>();
                  if (ds.name == old_name)
                  {
                    explorer.removeNode(it);
                    break;
                  }
                }

                explorer.addDevice(std::move(new_node));
              }
          });
        }
      }
    }
  }
}

void ApplicationPlugin::prepareNewDocument()
{
  on_stop();
}

void ApplicationPlugin::on_play(bool b, ::TimeVal t)
{
  // TODO have a on_exit handler to properly stop the scenario.
  if (auto doc = currentDocument())
  {
    if (auto pres = doc->presenter())
    {
      auto scenar = dynamic_cast<Scenario::ScenarioDocumentPresenter*>(
          pres->presenterDelegate());
      if (scenar)
        on_play(scenar->displayedElements.interval(), b, {}, t);
    }
    else
    {
      auto& mod = doc->model().modelDelegate();
      auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
      if (scenar)
      {
        on_play(scenar->baseInterval(), b, {}, t);
      }
    }
  }
}

void ApplicationPlugin::on_transport(TimeVal t)
{
  if (!m_clock)
    return;

  auto itv = m_clock->scenario.baseInterval().OSSIAInterval();
  if (!itv)
    return;

  auto& settings = context.settings<Execution::Settings::Model>();
  auto& ctx = m_clock->context;
  if(settings.getTransportValueCompilation())
  {
    auto execState = m_clock->context.execState;
    ctx.executionQueue.enqueue(
        [execState, itv, time = m_clock->context.time(t)] {
      itv->offset(time);
      execState->commit();
    });
  }
  else
  {
    ctx.executionQueue.enqueue(
        [itv, time = m_clock->context.time(t)] { itv->transport(time); });
  }
}



void ApplicationPlugin::on_play(
    Scenario::IntervalModel& cst,
    bool b,
    exec_setup_fun setup_fun,
    TimeVal t)
{
  auto doc = currentDocument();
  if (!doc)
    return;

  auto plugmodel
      = doc->context().findPlugin<Execution::DocumentPlugin>();
  if (!plugmodel)
    return;

  if (b)
  {
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
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      // Disable listening for everything
      if (explorer
          && !doc->context()
                  .app.settings<Execution::Settings::Model>()
                  .getExecutionListening())
      {
        explorer->deviceModel().listening().stop();
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
  else
  {
    if (m_clock)
    {
      m_clock->pause();
      m_paused = true;

      doc->context().execTimer.stop();
    }
  }
}

void ApplicationPlugin::on_record(::TimeVal t)
{
  SCORE_ASSERT(!m_playing);

  // TODO have a on_exit handler to properly stop the scenario.
  if (auto doc = currentDocument())
  {
    auto plugmodel
        = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;
    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &doc->model().modelDelegate());
    if (!scenar)
      return;

    // Listening isn't stopped here.
    plugmodel->reload(scenar->baseInterval());
    m_clock = makeClock(plugmodel->context());
    m_clock->play(t);

    doc->context().execTimer.start();
    m_playing = true;
    m_paused = false;
  }
}

void ApplicationPlugin::on_stop()
{
  if (audio)
  {
    audio->reload(nullptr);
  }
  m_playing = false;
  m_paused = false;

  if (m_clock)
  {
    m_clock->stop();
    m_clock.reset();
  }

  if (auto doc = currentDocument())
  {
    doc->context().execTimer.stop();
    auto plugmodel
        = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;
    else
    {
      // plugmodel->clear();
    }
    // If we can we resume listening
    if (!context.docManager.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }

    QTimer::singleShot(50, this, [this] {
      auto doc = currentDocument();
      if (!doc)
        return;
      auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
          &doc->model().modelDelegate());
      if (!scenar)
        return;
      scenar->baseInterval().reset();
      scenar->baseInterval().executionFinished();
      auto procs = doc->findChildren<Scenario::ProcessModel*>();
      for (Scenario::ProcessModel* e : procs)
      {
        for (auto& itv : e->intervals)
        {
          itv.reset();
          itv.executionFinished();
        }
        for (auto& ev : e->events)
        {
          ev.setStatus(Scenario::ExecutionStatus::Editing, *e);
        }
      }

      auto loops = doc->findChildren<Loop::ProcessModel*>();
      for (Loop::ProcessModel* lp : loops)
      {
        lp->interval().reset();
        lp->interval().executionFinished();
        lp->startEvent().setStatus(Scenario::ExecutionStatus::Editing, *lp);
        lp->endEvent().setStatus(Scenario::ExecutionStatus::Editing, *lp);
        lp->startState().setStatus(Scenario::ExecutionStatus::Editing);
        lp->endState().setStatus(Scenario::ExecutionStatus::Editing);
      }
    });
  }
}

void ApplicationPlugin::on_init()
{
  if (auto doc = currentDocument())
  {
    auto plugmodel
        = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;

    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &doc->model().modelDelegate());
    if (!scenar)
      return;

    auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
    // Disable listening for everything
    if (explorer
        && !doc->context()
                .app.settings<Execution::Settings::Model>()
                .getExecutionListening())
      explorer->deviceModel().listening().stop();

    auto state = Engine::score_to_ossia::state(
        scenar->baseScenario().startState(), plugmodel->context());
    state.launch();

    // If we can we resume listening
    if (!context.docManager.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }
  }
}

void ApplicationPlugin::initLocalTreeNodes(LocalTree::DocumentPlugin& lt)
{
  auto& appplug = *this;
  auto& root = lt.device().get_root_node();

  {
    auto n = root.create_child("running");
    auto p = n->create_parameter(ossia::val_type::BOOL);
    p->set_value(false);
    p->set_access(ossia::access_mode::GET);

    if (context.applicationSettings.gui)
    {
      auto& play_action = appplug.context.actions.action<Actions::Play>();
      connect(play_action.action(), &QAction::triggered, &lt, [=] {
        p->push_value(true);
      });

      auto& stop_action = context.actions.action<Actions::Stop>();
      connect(stop_action.action(), &QAction::triggered, &lt, [=] {
        p->push_value(false);
      });
    }
  }
  {
    auto local_play_node = root.create_child("play");
    auto local_play_address
        = local_play_node->create_parameter(ossia::val_type::BOOL);
    local_play_address->set_value(bool{false});
    local_play_address->set_access(ossia::access_mode::SET);
    local_play_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [=] {
        if (auto val = v.target<bool>())
        {
          if (!playing() && *val)
          {
            // not playing, play requested
            if (context.applicationSettings.gui)
            {
              auto& play_action = context.actions.action<Actions::Play>();
              play_action.action()->trigger();
            }
            else
            {
              this->on_play(true);
            }
          }
          else if (playing())
          {
            if (paused() == *val)
            {
              // paused, play requested
              // or playing, pause requested

              if (context.applicationSettings.gui)
              {
                auto& play_action = context.actions.action<Actions::Play>();
                play_action.action()->trigger();
              }
              else
              {
                this->on_play(true);
              }
            }
          }
        }
      });
    });
  }

  {
    auto local_play_node = root.create_child("global_play");
    auto local_play_address
        = local_play_node->create_parameter(ossia::val_type::BOOL);
    local_play_address->set_value(bool{false});
    local_play_address->set_access(ossia::access_mode::SET);
    local_play_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [=] {
        if (auto val = v.target<bool>())
        {
          if (!playing() && *val)
          {
            // not playing, play requested
            if (context.applicationSettings.gui)
            {
              auto& play_action
                  = context.actions.action<Actions::PlayGlobal>();
              play_action.action()->trigger();
            }
            else
            {
              if (auto doc = currentDocument())
              {
                auto& mod = doc->model().modelDelegate();
                auto scenar
                    = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
                if (scenar)
                {
                  on_play(scenar->baseInterval(), true, {}, TimeVal{});
                }
              }
            }
          }
          else if (playing())
          {
            if (paused() == *val)
            {
              // paused, play requested
              // or playing, pause requested

              if (context.applicationSettings.gui)
              {
                auto& play_action
                    = context.actions.action<Actions::PlayGlobal>();
                play_action.action()->trigger();
              }
              else
              {
                if (auto doc = currentDocument())
                {
                  auto& mod = doc->model().modelDelegate();
                  auto scenar
                      = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
                  if (scenar)
                  {
                    on_play(scenar->baseInterval(), true, {}, TimeVal{});
                  }
                }
              }
            }
          }
        }
      });
    });
  }

  {
    auto local_transport_node = root.create_child("transport");
    auto local_transport_address
        = local_transport_node->create_parameter(ossia::val_type::FLOAT);
    local_transport_address->set_value(bool{false});
    local_transport_address->set_access(ossia::access_mode::SET);
    local_transport_address->set_unit(ossia::millisecond_u{});
    local_transport_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [=] {
        on_transport(TimeVal::fromMsecs(ossia::convert<float>(v)));
      });
    });
  }

  {
    auto local_stop_node = root.create_child("stop");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          auto& stop_action = context.actions.action<Actions::Stop>();
          stop_action.action()->trigger();
        }
        else
        {
          this->on_stop();
        }
      });
    });
  }

  {
    auto local_stop_node = root.create_child("reinit");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          auto& stop_action = context.actions.action<Actions::Reinitialize>();
          stop_action.action()->trigger();
        }
        else
        {
          this->on_init();
        }
      });
    });
  }
  {
    auto node = root.create_child("exit");
    auto address = node->create_parameter(ossia::val_type::IMPULSE);
    address->set_value(ossia::impulse{});
    address->set_access(ossia::access_mode::SET);
    address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          auto& stop_action = context.actions.action<Actions::Stop>();
          stop_action.action()->trigger();
        }
        else
        {
          this->on_stop();
        }

        QTimer::singleShot(500, [] {
          auto pres = qApp->findChild<score::Presenter*>();
          pres->exit();
          QTimer::singleShot(500, [] { QCoreApplication::quit(); });
        });
      });
    });
  }
}

std::unique_ptr<Execution::Clock>
ApplicationPlugin::makeClock(const Execution::Context& ctx)
{
  auto& s = context.settings<Execution::Settings::Model>();
  return s.makeClock(ctx);
}
}
