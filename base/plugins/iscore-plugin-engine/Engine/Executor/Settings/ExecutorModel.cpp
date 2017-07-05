#include <ossia/editor/state/state_element.hpp>
#include "ExecutorModel.hpp"
#include <Engine/Executor/ClockManager/DefaultClockManager.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <Engine/iscore2OSSIA.hpp>
namespace Engine
{
namespace Execution
{
namespace Settings
{

namespace Parameters
{
const iscore::sp<ModelRateParameter> Rate{
    QStringLiteral("iscore_plugin_engine/ExecutionRate"), 50};
const iscore::sp<ModelClockParameter> Clock{
    QStringLiteral("iscore_plugin_engine/Clock"),
    ControlClockFactory::static_concreteKey()};
const iscore::sp<ModelExecutionListeningParameter> ExecutionListening{
    QStringLiteral("iscore_plugin_engine/ExecListening"), true};

static auto list()
{
  return std::tie(Rate, Clock, ExecutionListening);
}
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx)
    : m_clockFactories{ctx.interfaces<ClockManagerFactoryList>()}
{
  iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

std::unique_ptr<ClockManager>
Model::makeClock(const Engine::Execution::Context& ctx) const
{
  auto it = m_clockFactories.find(m_Clock);
  return it != m_clockFactories.end()
             ? it->make(ctx)
             : std::make_unique<ControlClock>(ctx);
}

std::function<ossia::time_value(const TimeVal&)>
Model::makeTimeFunction() const
{
  auto it = m_clockFactories.find(m_Clock);
  return it != m_clockFactories.end()
      ? it->makeTimeFunction()
      : &iscore_to_ossia::time;
}

ISCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
ISCORE_SETTINGS_PARAMETER_CPP(ClockManagerFactory::ConcreteKey, Model, Clock)
ISCORE_SETTINGS_PARAMETER_CPP(bool, Model, ExecutionListening)
}
}
}
