#pragma once
#include <Audio/AudioTick.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

namespace Execution
{
class DocumentPlugin;
class BaseScenarioElement;
}
namespace Execution
{
using tick_fun = ossia::audio_engine::fun_type;

tick_fun makeExecutionTick(
    ossia::tick_setup_options opt,
    Execution::DocumentPlugin& plug,
    Execution::BaseScenarioElement& cur);

tick_fun makeBenchmarkTick(
    ossia::tick_setup_options opt,
    Execution::DocumentPlugin& plug,
    Execution::BaseScenarioElement& cur);
}
