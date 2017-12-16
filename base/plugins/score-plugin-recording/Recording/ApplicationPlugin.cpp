// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationPlugin.hpp"

#include <QAction>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <score/actions/Menu.hpp>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>
#include <Recording/Record/RecordManager.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <score/application/ApplicationContext.hpp>

#include <core/document/Document.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <QApplication>
#include <QMainWindow>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedIntervals.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/widgets/SetIcons.hpp>
#include <core/application/ApplicationSettings.hpp>

namespace Recording
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  using namespace Scenario;
  // Since we have declared the dependency, we can assume
  // that ScenarioApplicationPlugin is instantiated already.
  auto& scenario_plugin
      = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
  connect(
      &scenario_plugin.execution(), &ScenarioExecution::startRecording, this,
      &ApplicationPlugin::record);
  connect(
      &scenario_plugin.execution(), &ScenarioExecution::startRecordingMessages,
      this, &ApplicationPlugin::recordMessages);
  connect(
      &scenario_plugin.execution(),
      &ScenarioExecution::stopRecording, // TODO this seems useless
      this, &ApplicationPlugin::stopRecord);

  m_ossiaplug = &ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();


  if(ctx.applicationSettings.gui)
  {
    auto& stop_action = ctx.actions.action<Actions::Stop>();
    m_stopAction = stop_action.action();
    connect(m_stopAction, &QAction::triggered, this, [&] { stopRecord(); });
  }
}

void ApplicationPlugin::record(
    Scenario::ProcessModel& scenar, Scenario::Point pt)
{
  if (m_currentContext)
    return;

  m_stopAction->trigger();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  m_currentContext = std::make_unique<Recording::RecordContext>(scenar, pt);
  m_recManager = std::make_unique<SingleRecorder<AutomationRecorder>>(
      *m_currentContext);

  if (context.settings<Curve::Settings::Model>().getPlayWhileRecording())
  {
    connect(
        &m_recManager->recorder,
        &Recording::AutomationRecorder::firstMessageReceived, this,
        [=]() { m_ossiaplug->on_record(pt.date); }, Qt::QueuedConnection);
  }

  auto res = m_recManager->setup();
  if(!res)
  {
    m_recManager.reset();
    m_currentContext.reset();
  }
}

void ApplicationPlugin::recordMessages(
    Scenario::ProcessModel& scenar, Scenario::Point pt)
{
  if (m_currentContext)
    return;

  m_stopAction->trigger();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  m_currentContext = std::make_unique<Recording::RecordContext>(scenar, pt);
  m_recMessagesManager
      = std::make_unique<SingleRecorder<MessageRecorder>>(*m_currentContext);

  if (context.settings<Curve::Settings::Model>().getPlayWhileRecording())
  {
    connect(
        &m_recMessagesManager->recorder,
        &Recording::MessageRecorder::firstMessageReceived, this,
        [=]() { m_ossiaplug->on_record(pt.date); }, Qt::QueuedConnection);
  }

  auto res = m_recMessagesManager->setup();
  if(!res)
  {
    m_recMessagesManager.reset();
    m_currentContext.reset();
  }
}

void ApplicationPlugin::stopRecord()
{
  if (m_recManager)
  {
    m_recManager->stop();
    m_recManager.reset();
  }

  if (m_recMessagesManager)
  {
    m_recMessagesManager->stop();
    m_recMessagesManager.reset();
  }

  m_currentContext.reset();
}
}
