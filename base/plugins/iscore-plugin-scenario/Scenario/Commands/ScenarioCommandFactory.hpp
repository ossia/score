#pragma once
#include <iscore/command/Command.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class BaseScenario;
class ProcessModel;
namespace Command
{

ISCORE_PLUGIN_SCENARIO_EXPORT const CommandGroupKey&
ScenarioCommandFactoryName();
}

} // namespace Scenario

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT const CommandGroupKey&
CommandFactoryName<Scenario::ProcessModel>();

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT const CommandGroupKey&
CommandFactoryName<Scenario::BaseScenario>();
