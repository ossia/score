#pragma once

namespace Process
{
class Cable;
class Inlet;
class Outlet;
class ProcessModel;
struct Preset;
}

namespace Library
{
struct ProcessData;
}

namespace Scenario
{
class ScenarioDocumentPresenter;

void createProcessInCable(
    Scenario::ScenarioDocumentPresenter& parent, const Library::ProcessData& dat,
    const Process::Cable& cbl);
void loadPresetInCable(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::Cable& cbl);

void createProcessBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Library::ProcessData& dat,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p);
void loadPresetBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p);

void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Library::ProcessData& dat,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p);
void loadPresetAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p);

}
