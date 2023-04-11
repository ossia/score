#pragma once
#include <Process/ProcessFlags.hpp>

#include <score_plugin_scenario_export.h>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class EventModel;
class IntervalModel;
class TimeSyncModel;
class ProcessModel;

SCORE_PLUGIN_SCENARIO_EXPORT
Process::NetworkFlags networkFlags(const Process::ProcessModel&) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
Process::NetworkFlags networkFlags(const Scenario::EventModel&) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
Process::NetworkFlags networkFlags(const Scenario::IntervalModel&) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
Process::NetworkFlags networkFlags(const Scenario::TimeSyncModel&) noexcept;

SCORE_PLUGIN_SCENARIO_EXPORT
QString networkGroup(const Process::ProcessModel&) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
QString networkGroup(const Scenario::EventModel&) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
QString networkGroup(const Scenario::IntervalModel&) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
QString networkGroup(const Scenario::TimeSyncModel&) noexcept;

}
