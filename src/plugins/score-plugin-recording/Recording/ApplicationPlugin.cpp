// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationPlugin.hpp"

#include <Curve/Settings/CurveSettingsModel.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Recording/Record/RecordTools.hpp>
#include <Recording/Session/RecordSession.hpp>
#include <Recording/Settings/RecordingSettings.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>

#include <QAction>
#include <QApplication>

namespace Recording
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  using namespace Scenario;
  // Since we have declared the dependency, we can assume
  // that ScenarioApplicationPlugin is instantiated already.
  auto& scenario_plugin = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
  connect(
      &scenario_plugin.execution(), &ScenarioExecution::startRecording, this,
      &ApplicationPlugin::record, Qt::QueuedConnection);
  connect(
      &scenario_plugin.execution(), &ScenarioExecution::startRecordingMessages, this,
      &ApplicationPlugin::recordMessages, Qt::QueuedConnection);
  connect(
      &scenario_plugin.execution(), &ScenarioExecution::stopRecording, this,
      &ApplicationPlugin::stopSession);

  m_ossiaplug = &ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();

  if(ctx.applicationSettings.gui)
  {
    auto& stop_action = ctx.actions.action<Actions::Stop>();
    m_stopAction = stop_action.action();
    connect(m_stopAction, &QAction::triggered, this, [this] { stopSession(); });
  }
}

ApplicationPlugin::~ApplicationPlugin() = default;

RecordSession* ApplicationPlugin::adopt(
    std::unique_ptr<RecordSession> session, const score::DocumentContext& ctx)
{
  if(session->sources() == 0 || !session->start())
    return nullptr;

  m_session = std::move(session);
  m_sessionDocument = &ctx.document;
  // Destroyed once it has placed its takes, out of its own signal
  connect(m_session.get(), &RecordSession::finished, this, [this] {
    QMetaObject::invokeMethod(this, [this] {
      if(m_session && !m_session->running())
      {
        m_session.reset();
        m_sessionDocument = nullptr;
      }
    }, Qt::QueuedConnection);
  });
  return m_session.get();
}

RecordSession* ApplicationPlugin::startSession(
    const score::DocumentContext& ctx, const std::vector<const Process::Port*>& ports,
    RecordOptions options)
{
  if(m_session)
    return nullptr;
  auto session = std::make_unique<RecordSession>(ctx, options);
  for(auto* p : ports)
    if(p)
      session->addSource(*p);
  return adopt(std::move(session), ctx);
}

RecordSession* ApplicationPlugin::recordAddresses(
    Scenario::ProcessModel& scenario, Scenario::Point pt,
    const std::vector<State::Address>& addresses, AddressPlacement placement)
{
  if(m_session)
    return nullptr;
  auto& ctx = score::IDocument::documentContext(scenario);
  const auto& curves = context.settings<Curve::Settings::Model>();

  // Recorded from the point: playback stops, and starts from there with the
  // first value (when the settings say so; else when the user plays)
  if(auto* exec = ctx.findPlugin<Execution::DocumentPlugin>(); exec && exec->isPlaying())
  {
    m_ossiaplug->execution().request_stop();
    // What the stop reports is not for the session
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if(exec->isPlaying())
      return nullptr;
  }

  RecordOptions options = defaultOptions();
  options.start = RecordOptions::Start::FirstEvent;
  auto session = std::make_unique<RecordSession>(ctx, options);
  for(const auto& addr : addresses)
    session->addAddress(addr);

  CurveOptions c;
  c.stepped = curves.getCurveMode() == Curve::Settings::Mode::Parameter;
  c.simplify = curves.getSimplify();
  c.simplificationRatio = curves.getSimplificationRatio();
  session->placeInNewBox(scenario, pt, placement, c, curves.getPlayWhileRecording());
  return adopt(std::move(session), ctx);
}

void ApplicationPlugin::record(Scenario::ProcessModel* scenar, Scenario::Point pt)
{
  if(!scenar)
    return;
  auto& ctx = score::IDocument::documentContext(*scenar);
  recordAddresses(
      *scenar, pt, addressesToRecord(Explorer::deviceExplorerFromContext(ctx)),
      AddressPlacement::Automations);
}

void ApplicationPlugin::recordMessages(Scenario::ProcessModel* scenar, Scenario::Point pt)
{
  if(!scenar)
    return;
  auto& ctx = score::IDocument::documentContext(*scenar);
  recordAddresses(
      *scenar, pt, addressesToRecord(Explorer::deviceExplorerFromContext(ctx)),
      AddressPlacement::Messages);
}

void ApplicationPlugin::stopSession()
{
  if(m_session)
    m_session->stop();
}

bool ApplicationPlugin::recording(const score::DocumentContext& ctx) const noexcept
{
  return m_session && m_session->running() && m_sessionDocument == &ctx.document;
}

RecordOptions ApplicationPlugin::defaultOptions() const
{
  if(auto* s = context.findSettings<Settings::Model>())
    return s->options();
  return RecordOptions{};
}

void ApplicationPlugin::on_closeDocument(score::Document& doc)
{
  // The session refers to the document: it ends, its takes placed, first
  if(m_session && m_sessionDocument == &doc)
  {
    m_session->stop();
    m_session.reset();
    m_sessionDocument = nullptr;
  }
}
}
