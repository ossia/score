#pragma once
#include <State/Expression.hpp>
#include <State/Value.hpp>

#include <Process/TimeValue.hpp>

#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <score_plugin_scenario_export.h>

#include <memory>
namespace Execution
{
struct Context;
}
namespace Scenario
{
class StateModel;
}
namespace ossia
{
struct execution_state;
class state;
} // namespace OSSIA
namespace Engine
{
namespace score_to_ossia
{

//! When played, a state leaves out the addresses that the processes of the
//! interval it starts take over (an automation with tween). Played from the
//! UI, it sends them all.
enum class StatePlay
{
  Execution,
  FromUI
};

void state(
    ossia::state& ossia_state, const Scenario::StateModel& score_state,
    const ossia::execution_state& ctx, StatePlay play = StatePlay::FromUI);

SCORE_PLUGIN_SCENARIO_EXPORT
ossia::state state(
    const Scenario::StateModel& score_state, const ossia::execution_state& ctx,
    StatePlay play = StatePlay::FromUI);

SCORE_PLUGIN_SCENARIO_EXPORT
void play_state_from_ui(
    const Scenario::StateModel& score_state, const Execution::Context& ctx);

ossia::expression_ptr
condition_expression(const State::Expression& expr, const ossia::execution_state&);
ossia::expression_ptr
trigger_expression(const State::Expression& expr, const ossia::execution_state&);
}
}
