// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/state/state_element.hpp>
#include "ExecutorModel.hpp"
#include <Engine/Executor/ClockManager/DefaultClockManager.hpp>
#include <Engine/Executor/Dataflow/DataflowClock.hpp>
#include <score/application/ApplicationContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Engine/OSSIA2score.hpp>
namespace Engine
{
namespace Execution
{
namespace Settings
{

namespace Parameters
{
const score::sp<ModelRateParameter> Rate{
    QStringLiteral("score_plugin_engine/ExecutionRate"), 50};
const score::sp<ModelClockParameter> Clock{
    QStringLiteral("score_plugin_engine/Clock"),
    ControlClockFactory::static_concreteKey()};
const score::sp<ModelExecutionListeningParameter> ExecutionListening{
    QStringLiteral("score_plugin_engine/ExecListening"), true};

static auto list()
{
  return std::tie(Rate, Clock, ExecutionListening);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
    : m_clockFactories{ctx.interfaces<ClockManagerFactoryList>()}
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

std::unique_ptr<ClockManager>
Model::makeClock(const Engine::Execution::Context& ctx) const
{
    return std::make_unique<Dataflow::Clock>(ctx);
}

std::function<ossia::time_value(const TimeVal&)>
Model::makeTimeFunction(const score::DocumentContext& ctx) const
{
  auto it = m_clockFactories.find(m_Clock);
  return it != m_clockFactories.end()
      ? it->makeTimeFunction(ctx)
      : &score_to_ossia::defaultTime;
}

std::function<TimeVal(const ossia::time_value&)>
Model::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  auto it = m_clockFactories.find(m_Clock);
  return it != m_clockFactories.end()
               ? it->makeReverseTimeFunction(ctx)
               : &ossia_to_score::defaultTime;
}

SCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(ClockManagerFactory::ConcreteKey, Model, Clock)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, ExecutionListening)
}
}
}
