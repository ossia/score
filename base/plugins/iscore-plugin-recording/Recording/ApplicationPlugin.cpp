#include "ApplicationPlugin.hpp"

#include <QAction>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/actions/Menu.hpp>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>
#include <Recording/Record/RecordManager.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <iscore/application/ApplicationContext.hpp>

#include <core/document/Document.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>

#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <QApplication>
#include <QMainWindow>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>
#include <iscore/actions/ActionManager.hpp>
#include <iscore/widgets/SetIcons.hpp>

namespace Recording
{
ApplicationPlugin::ApplicationPlugin(const iscore::GUIApplicationContext& ctx)
    : iscore::GUIApplicationPlugin{ctx}
{
  using namespace Scenario;
  // Since we have declared the dependency, we can assume
  // that ScenarioApplicationPlugin is instantiated already.
  auto& scenario_plugin
      = ctx.applicationPlugin<ScenarioApplicationPlugin>();
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

  m_ossiaplug = &ctx.applicationPlugin<Engine::ApplicationPlugin>();

  auto& stop_action = ctx.actions.action<Actions::Stop>();
  m_stopAction = stop_action.action();
  connect(m_stopAction, &QAction::triggered, this, [&] { stopRecord(); });
}

void ApplicationPlugin::record(
    const Scenario::ProcessModel& scenar, Scenario::Point pt)
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
    const Scenario::ProcessModel& scenar, Scenario::Point pt)
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
