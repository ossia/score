// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationPlugin.hpp"

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Process/TimeValue.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/tools/Todo.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/context.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <Engine/Executor/ContextMenu/PlayContextMenu.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
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
#include <score/actions/ActionManager.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>
#include <Engine/OssiaLogger.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <QApplication>

#include <QAction>
#include <QVariant>
#include <QVector>
#include <spdlog/spdlog.h>
namespace Engine
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}, m_playActions{*this, ctx}
{
  std::vector<spdlog::sink_ptr> v{
    spdlog::sinks::stderr_sink_mt::instance(),
    std::make_shared<OssiaLogger>()};

  ossia::context context{v};
  ossia::logger().set_level(spdlog::level::debug);
  // Two parts :
  // One that maintains the devices for each document
  // (and disconnects / reconnects them when the current document changes)
  // Also during execution, one shouldn't be able to switch document.

  // Another part that, at execution time, creates structures corresponding
  // to the Scenario plug-in with the OSSIA API.

  if(ctx.applicationSettings.gui)
  {
    auto& play_action = ctx.actions.action<Actions::Play>();
    connect(
          play_action.action(), &QAction::triggered, this,
          [&](bool b) { on_play(b); }, Qt::QueuedConnection);

    auto& stop_action = ctx.actions.action<Actions::Stop>();
    connect(
          stop_action.action(), &QAction::triggered, this,
          &ApplicationPlugin::on_stop, Qt::QueuedConnection);

    auto& init_action = ctx.actions.action<Actions::Reinitialize>();
    connect(
          init_action.action(), &QAction::triggered, this,
          &ApplicationPlugin::on_init, Qt::QueuedConnection);

    auto& ctrl = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    con(ctrl.execution(), &Scenario::ScenarioExecution::playAtDate, this,
        [ =, act = play_action.action() ](const TimeVal& t) {
      on_play(true, t);
      act->trigger();
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

void ApplicationPlugin::on_initDocument(score::Document& doc)
{
  doc.model().addPluginModel(new Engine::LocalTree::DocumentPlugin{
      doc.context(), getStrongId(doc.model().pluginModels()), &doc.model()});
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  LocalTree::DocumentPlugin* lt = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if (lt)
  {
    lt->init();
    initLocalTreeNodes(*lt);
  }
  doc.model().addPluginModel(new Engine::Execution::DocumentPlugin{
      doc.context(), getStrongId(doc.model().pluginModels()), &doc.model()});
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
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
    if(auto pres = doc->presenter())
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
      if(scenar)
      {
        on_play(scenar->baseInterval(), b, {}, t);
      }
    }
  }
}

void ApplicationPlugin::on_play(
    Scenario::IntervalModel& cst, bool b,
    std::function<void(const Engine::Execution::Context&)> setup_fun,
    TimeVal t)
{
  auto doc = currentDocument();
  if(!doc)
    return;

  auto plugmodel
      = doc->context().findPlugin<Engine::Execution::DocumentPlugin>();
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
      if (explorer && !doc->context().app.settings<Execution::Settings::Model>().getExecutionListening())
      {
        explorer->deviceModel().listening().stop();
      }

      plugmodel->reload(cst);

      auto& c = plugmodel->context();
      m_clock = makeClock(c);

      if(setup_fun)
      {
        plugmodel->runAllCommands();
        setup_fun(c);
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
        = doc->context().findPlugin<Engine::Execution::DocumentPlugin>();
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
        = doc->context().findPlugin<Engine::Execution::DocumentPlugin>();
    if (!plugmodel)
      return;
    else
    {
      //plugmodel->clear();
    }
    // If we can we resume listening
    if (!context.docManager.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }
  }
}

void ApplicationPlugin::on_init()
{
  if (auto doc = currentDocument())
  {
    auto plugmodel
        = doc->context().findPlugin<Engine::Execution::DocumentPlugin>();
    if (!plugmodel)
      return;

    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &doc->model().modelDelegate());
    if (!scenar)
      return;

    auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
    // Disable listening for everything
    if (explorer && !doc->context().app.settings<Execution::Settings::Model>().getExecutionListening())
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


    if(context.applicationSettings.gui)
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
      QMetaObject::invokeMethod(this, [&] {
      if (auto val = v.target<bool>())
      {
        if (!appplug.playing() && *val)
        {
          // not playing, play requested
          if(context.applicationSettings.gui)
          {
            auto& play_action = appplug.context.actions.action<Actions::Play>();
            play_action.action()->trigger();
          }
          else
          {
            this->on_play(true);
          }
        }
        else if (appplug.playing())
        {
          if (appplug.paused() == *val)
          {
            // paused, play requested
            // or playing, pause requested

            if(context.applicationSettings.gui)
            {
              auto& play_action
                  = appplug.context.actions.action<Actions::Play>();
              play_action.action()->trigger();
            }
            else
            {
              this->on_play(true);
            }
          }
        }
      }
      }, Qt::QueuedConnection);
    });
  }

  {
    auto local_stop_node = root.create_child("stop");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      QMetaObject::invokeMethod(this, [&] {
      if(context.applicationSettings.gui)
      {
        auto& stop_action = appplug.context.actions.action<Actions::Stop>();
        stop_action.action()->trigger();
      }
      else
      {
        this->on_stop();
      }
      }, Qt::QueuedConnection);
    });

  }
  {
    auto node = root.create_child("exit");
    auto address = node->create_parameter(ossia::val_type::IMPULSE);
    address->set_value(ossia::impulse{});
    address->set_access(ossia::access_mode::SET);
    address->add_callback([&](const ossia::value&) {
      QMetaObject::invokeMethod(this, [&] {
      if(context.applicationSettings.gui)
      {
        auto& stop_action = appplug.context.actions.action<Actions::Stop>();
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
      }, Qt::QueuedConnection);
    });
  }
}

std::unique_ptr<Engine::Execution::ClockManager>
ApplicationPlugin::makeClock(const Engine::Execution::Context& ctx)
{
  auto& s = context.settings<Engine::Execution::Settings::Model>();
  return s.makeClock(ctx);
}
}
