#pragma once
#include <Audio/AudioTick.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>

#include <score_plugin_engine_export.h>

namespace Execution
{
class DocumentPlugin;
class BaseScenarioElement;
}
namespace Execution
{
using tick_fun = ossia::audio_engine::fun_type;

SCORE_PLUGIN_ENGINE_EXPORT
tick_fun makeExecutionTick(
    ossia::tick_setup_options opt, Execution::DocumentPlugin& plug,
    const std::shared_ptr<Execution::BaseScenarioElement>& scenar);

}
