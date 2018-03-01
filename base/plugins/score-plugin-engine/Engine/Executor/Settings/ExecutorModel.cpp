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
const score::sp<ModelClockParameter> Clock{
    QStringLiteral("score_plugin_engine/Clock"),
    ControlClockFactory::static_concreteKey()};

const score::sp<ModelRateParameter> Rate{
  QStringLiteral("score_plugin_engine/Rate"), 50};

const score::sp<ModelSchedulingParameter> Scheduling{
    QStringLiteral("score_plugin_engine/Scheduling"), SchedulingPolicies{}.StaticTC};

const score::sp<ModelOrderingParameter> Ordering{
    QStringLiteral("score_plugin_engine/Ordering"), OrderingPolicies{}.CreationOrder};

const score::sp<ModelMergingParameter> Merging{
    QStringLiteral("score_plugin_engine/Merging"), MergingPolicies{}.Merge};

const score::sp<ModelCommitParameter> Commit{
    QStringLiteral("score_plugin_engine/Commit"), CommitPolicies{}.Merged};

const score::sp<ModelTickParameter> Tick{
  QStringLiteral("score_plugin_engine/Tick"), TickPolicies{}.Buffer};

const score::sp<ModelParallelParameter> Parallel{
    QStringLiteral("score_plugin_engine/Parallel"), true};

const score::sp<ModelExecutionListeningParameter> ExecutionListening{
    QStringLiteral("score_plugin_engine/ExecListening"), true};

const score::sp<ModelLoggingParameter> Logging{
  QStringLiteral("score_plugin_engine/Logging"), true};

const score::sp<ModelLoggingParameter> ScoreOrder{
  QStringLiteral("score_plugin_engine/ScoreOrder"), false};

static auto list()
{
  return std::tie( Clock, Rate, Scheduling, Ordering, Merging, Commit, Tick, Parallel, ExecutionListening, Logging, ScoreOrder);
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

time_function
Model::makeTimeFunction(const score::DocumentContext& ctx) const
{
  auto it = m_clockFactories.find(m_Clock);
  return it != m_clockFactories.end()
      ? it->makeTimeFunction(ctx)
      : &score_to_ossia::defaultTime;
}

reverse_time_function
Model::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  auto it = m_clockFactories.find(m_Clock);
  return it != m_clockFactories.end()
               ? it->makeReverseTimeFunction(ctx)
               : &ossia_to_score::defaultTime;
}

SCORE_SETTINGS_PARAMETER_CPP(ClockManagerFactory::ConcreteKey, Model, Clock)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Scheduling)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Ordering)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Merging)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Commit)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Tick)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Parallel)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, ExecutionListening)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Logging)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, ScoreOrder)
}
}
}
