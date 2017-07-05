#pragma once
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Process/TimeValue.hpp>
#include <iscore_plugin_engine_export.h>
namespace Engine
{
namespace Execution
{
namespace Settings
{
class ISCORE_PLUGIN_ENGINE_EXPORT Model : public iscore::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(int rate READ getRate WRITE setRate NOTIFY RateChanged)
  Q_PROPERTY(ClockManagerFactory::ConcreteKey clock READ getClock WRITE
                 setClock NOTIFY ClockChanged)
  Q_PROPERTY(bool executionListening READ getExecutionListening WRITE setExecutionListening NOTIFY ExecutionListeningChanged)

  int m_Rate{};
  ClockManagerFactory::ConcreteKey m_Clock;
  bool m_ExecutionListening{};

  const ClockManagerFactoryList& m_clockFactories;

public:
  Model(QSettings& set, const iscore::ApplicationContext& ctx);

  const ClockManagerFactoryList& clockFactories() const
  {
    return m_clockFactories;
  }

  std::unique_ptr<ClockManager>
  makeClock(const Engine::Execution::Context& ctx) const;
  std::function<ossia::time_value(const TimeVal&)>
  makeTimeFunction(const iscore::DocumentContext&) const;
  std::function<TimeVal(const ossia::time_value&)>
  makeReverseTimeFunction(const iscore::DocumentContext&) const;

  ISCORE_SETTINGS_PARAMETER_HPP(int, Rate)
  ISCORE_SETTINGS_PARAMETER_HPP(ClockManagerFactory::ConcreteKey, Clock)
  ISCORE_SETTINGS_PARAMETER_HPP(bool, ExecutionListening)
};

ISCORE_SETTINGS_PARAMETER(Model, Rate)
ISCORE_SETTINGS_PARAMETER(Model, Clock)
ISCORE_SETTINGS_PARAMETER(Model, ExecutionListening)
}
}
}
