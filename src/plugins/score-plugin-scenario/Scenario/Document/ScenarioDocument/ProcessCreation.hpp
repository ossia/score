#pragma once

#include <Process/TimeValue.hpp>

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

void createProcessInCable(
    const Process::Context& context, const Scenario::ScenarioDocumentModel& model,
    const Process::ProcessData& dat, std::optional<TimeVal>,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)>,
    const Process::Cable& cbl);

void loadPresetInCable(
    const Process::Context& context, const Scenario::ScenarioDocumentModel& model,
    const Process::Preset& dat, const Process::Cable& cbl);

void createProcessBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::ProcessData& dat,
    std::optional<TimeVal>,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)>,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p);
void loadPresetBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p);

void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::ProcessData& dat,
    std::optional<TimeVal>,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)>,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p);
void loadPresetAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p);

}
