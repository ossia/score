#pragma once
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Process/TimeValue.hpp>
#include <score_plugin_engine_export.h>
namespace Engine
{
namespace Execution
{
namespace Settings
{
struct SchedulingPolicies
{
  QString StaticFixed{"Static Fixed"};
  QString StaticBFS{"Static (BFS)"};
  QString StaticTC{"Static (TC)"};
  QString DynamicTemporal{"Dynamic (Temporal, Y)"};
  QString DynamicY{"Dynamic (Y)"};
  QString Dynamic{"Dynamic (Simple)"};
};
class SCORE_PLUGIN_ENGINE_EXPORT Model : public score::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(int rate READ getRate WRITE setRate NOTIFY RateChanged)
  Q_PROPERTY(ClockManagerFactory::ConcreteKey clock READ getClock WRITE setClock NOTIFY ClockChanged)
  Q_PROPERTY(bool executionListening READ getExecutionListening WRITE setExecutionListening NOTIFY ExecutionListeningChanged)

  Q_PROPERTY(QString scheduling READ getScheduling WRITE setScheduling NOTIFY SchedulingChanged)
  int m_Rate{};
  ClockManagerFactory::ConcreteKey m_Clock;
  QString m_Scheduling;
  bool m_ExecutionListening{};

  const ClockManagerFactoryList& m_clockFactories;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  const ClockManagerFactoryList& clockFactories() const
  {
    return m_clockFactories;
  }

  std::unique_ptr<ClockManager>
  makeClock(const Engine::Execution::Context& ctx) const;
  time_function
  makeTimeFunction(const score::DocumentContext&) const;
  reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext&) const;

  SCORE_SETTINGS_PARAMETER_HPP(int, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(ClockManagerFactory::ConcreteKey, Clock)
  SCORE_SETTINGS_PARAMETER_HPP(bool, ExecutionListening)
  SCORE_SETTINGS_PARAMETER_HPP(QString, Scheduling)
};

SCORE_SETTINGS_PARAMETER(Model, Rate)
SCORE_SETTINGS_PARAMETER(Model, Clock)
SCORE_SETTINGS_PARAMETER(Model, ExecutionListening)
SCORE_SETTINGS_PARAMETER(Model, Scheduling)
}
}
}
