#pragma once
#include <score/command/Command.hpp>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class BaseScenario;
class ProcessModel;
namespace Command
{

SCORE_PLUGIN_SCENARIO_EXPORT const CommandGroupKey& CommandFactoryName();
}

} // namespace Scenario

template <>
SCORE_PLUGIN_SCENARIO_EXPORT const CommandGroupKey& CommandFactoryName<Scenario::ProcessModel>();

template <>
SCORE_PLUGIN_SCENARIO_EXPORT const CommandGroupKey& CommandFactoryName<Scenario::BaseScenario>();
