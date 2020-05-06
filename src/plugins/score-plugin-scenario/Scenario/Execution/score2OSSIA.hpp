#pragma once
#include <Process/TimeValue.hpp>
#include <State/Expression.hpp>
#include <State/Value.hpp>

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

void state(
    ossia::state& ossia_state,
    const Scenario::StateModel& score_state,
    const Execution::Context& ctx);

SCORE_PLUGIN_SCENARIO_EXPORT
ossia::state
state(const Scenario::StateModel& score_state, const Execution::Context& ctx);

ossia::expression_ptr condition_expression(
    const State::Expression& expr,
    const ossia::execution_state&);
ossia::expression_ptr trigger_expression(
    const State::Expression& expr,
    const ossia::execution_state&);
}
}
