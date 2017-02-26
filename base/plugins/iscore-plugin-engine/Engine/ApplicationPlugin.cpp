#include "ApplicationPlugin.hpp"

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Process/TimeValue.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <iscore/tools/Todo.hpp>

#include <ossia/context.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <Engine/Executor/ContextMenu/PlayContextMenu.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <algorithm>
#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/actions/ActionManager.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>
#include <Engine/OssiaLogger.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>

#include <QAction>
#include <QVariant>
#include <QVector>
namespace Engine
{
ApplicationPlugin::ApplicationPlugin(const iscore::GUIApplicationContext& ctx)
    : iscore::GUIApplicationPlugin{ctx}, m_playActions{*this, ctx}
{
  std::vector<spdlog::sink_ptr> v{
    spdlog::sinks::stderr_sink_mt::instance(),
    std::make_shared<OssiaLogger>()};

  ossia::context context{v};
  // Two parts :
  // One that maintains the devices for each document
  // (and disconnects / reconnects them when the current document changes)
  // Also during execution, one shouldn't be able to switch document.

  // Another part that, at execution time, creates structures corresponding
  // to the Scenario plug-in with the OSSIA API.

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

  auto& ctrl = ctx.components
                   .applicationPlugin<Scenario::ScenarioApplicationPlugin>();
  con(ctrl.execution(), &Scenario::ScenarioExecution::playAtDate, this,
      [ =, act = play_action.action() ](const TimeVal& t) {
        on_play(true, t);
        act->trigger();
      });

  m_playActions.setupContextMenu(ctrl.layerContextMenuRegistrar());
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

void ApplicationPlugin::on_initDocument(iscore::Document& doc)
{
  doc.model().addPluginModel(new Engine::LocalTree::DocumentPlugin{
      doc, getStrongId(doc.model().pluginModels()), &doc.model()});
}

void ApplicationPlugin::on_createdDocument(iscore::Document& doc)
{
  LocalTree::DocumentPlugin* lt = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if (lt)
  {
    lt->init();
  }
  doc.model().addPluginModel(new Engine::Execution::DocumentPlugin{
      doc, getStrongId(doc.model().pluginModels()), &doc.model()});
}

void ApplicationPlugin::on_documentChanged(
    iscore::Document* olddoc, iscore::Document* newdoc)
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
    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &doc->model().modelDelegate());
    if (!scenar)
      return;
    on_play(scenar->displayedElements.constraint(), b, {}, t);
  }
}

void ApplicationPlugin::on_play(
    Scenario::ConstraintModel& cst, bool b,
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
      ISCORE_ASSERT(bool(m_clock));
      auto bs = plugmodel->baseScenario();
      ossia::time_constraint& cstr = *bs->baseConstraint().OSSIAConstraint();
      if (cstr.paused())
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
      if (explorer)
        explorer->deviceModel().listening().stop();

      plugmodel->reload(cst);

      auto& c = plugmodel->context();
      m_clock = makeClock(c);

      connect(
          plugmodel->baseScenario(),
          &Engine::Execution::BaseScenarioElement::finished, this,
          [=]() {
            auto& stop_action = context.actions.action<Actions::Stop>();
            stop_action.action()->trigger();
          },
          Qt::QueuedConnection);

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
  }
  else
  {
    if (m_clock)
    {
      m_clock->pause();
      m_paused = true;
    }
  }
}

void ApplicationPlugin::on_record(::TimeVal t)
{
  ISCORE_ASSERT(!m_playing);

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
    plugmodel->reload(scenar->baseConstraint());
    m_clock = makeClock(plugmodel->context());
    m_clock->play(t);

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
    auto plugmodel
        = doc->context().findPlugin<Engine::Execution::DocumentPlugin>();
    if (!plugmodel)
      return;

    if (plugmodel && plugmodel->baseScenario())
    {
      plugmodel->clear();
    }

    // If we can we resume listening
    if (!context.documents.preparingNewDocument())
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
    if (explorer)
      explorer->deviceModel().listening().stop();

    auto state = Engine::iscore_to_ossia::state(
        scenar->baseScenario().startState(), plugmodel->context());
    state.launch();

    // If we can we resume listening
    if (!context.documents.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }
  }
}

std::unique_ptr<Engine::Execution::ClockManager>
ApplicationPlugin::makeClock(const Engine::Execution::Context& ctx)
{
  auto& s = context.settings<Engine::Execution::Settings::Model>();
  return s.makeClock(ctx);
}
}
