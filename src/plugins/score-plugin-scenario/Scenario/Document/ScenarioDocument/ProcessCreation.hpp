#pragma once

#include <Process/TimeValue.hpp>

#include <score_plugin_scenario_export.h>

#include <functional>

namespace score
{
struct Dispatcher;
}
namespace Process
{
class Cable;
class Inlet;
class Outlet;
class ProcessModel;
struct Preset;
struct Context;
struct ProcessData;
}

namespace Scenario
{
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;

SCORE_PLUGIN_SCENARIO_EXPORT
void createProcessInCable(
    const Process::Context& context, const Scenario::ScenarioDocumentModel& model,
    const Process::ProcessData& dat, std::optional<TimeVal>,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)>,
    const Process::Cable& cbl);

SCORE_PLUGIN_SCENARIO_EXPORT
void loadPresetInCable(
    const Process::Context& context, const Scenario::ScenarioDocumentModel& model,
    const Process::Preset& dat, const Process::Cable& cbl);

SCORE_PLUGIN_SCENARIO_EXPORT
void createProcessBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::ProcessData& dat,
    std::optional<TimeVal>,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)>,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p);
SCORE_PLUGIN_SCENARIO_EXPORT
void loadPresetBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p);

SCORE_PLUGIN_SCENARIO_EXPORT
void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::ProcessData& dat,
    std::optional<TimeVal>,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)>,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p);
SCORE_PLUGIN_SCENARIO_EXPORT
void loadPresetAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p);

}
