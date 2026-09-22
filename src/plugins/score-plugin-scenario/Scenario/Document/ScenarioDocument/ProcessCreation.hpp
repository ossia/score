#pragma once

#include <State/Address.hpp>

#include <Process/Dataflow/PortType.hpp>
#include <Process/TimeValue.hpp>

#include <score_plugin_scenario_export.h>

#include <functional>
#include <optional>

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
namespace Command
{
class Macro;
}

//! First port of that type which carries signal flow: control ports are
//! First port of that type, preferring signal ports over control ones.
SCORE_PLUGIN_SCENARIO_EXPORT
const Process::Inlet*
firstInletOfType(const Process::ProcessModel& proc, Process::PortType type) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
const Process::Outlet*
firstOutletOfType(const Process::ProcessModel& proc, Process::PortType type) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
const Process::Outlet* firstSignalOutlet(const Process::ProcessModel& proc) noexcept;

//! What an outlet says about where its signal goes once it leaves the chain.
//! Gain and pan are not part of it: outlets chained in series both apply theirs,
//! so moving them would change the level.
struct OutletRouting
{
  State::AddressAccessor address;
  std::optional<bool> propagate;
};

SCORE_PLUGIN_SCENARIO_EXPORT
OutletRouting outletRouting(const Process::Outlet& p) noexcept;

SCORE_PLUGIN_SCENARIO_EXPORT
void applyOutletRouting(
    Command::Macro& m, const Process::Outlet& to, const OutletRouting& routing);

//! Whether insertProcessInCable would connect anything at all, for drop feedback.
SCORE_PLUGIN_SCENARIO_EXPORT
bool canInsertProcessInCable(
    const Process::Context& ctx, const Process::ProcessModel& proc,
    const Process::Cable& cbl);

//! Insert `proc` in `cbl` when it has both an inlet and an outlet of the
//! cable's type; when only one end matches, leave the cable alone and just
//! connect that end.
SCORE_PLUGIN_SCENARIO_EXPORT
void insertProcessInCable(
    score::Dispatcher& disp, const Process::Context& ctx,
    const Scenario::ScenarioDocumentModel& model, const Process::ProcessModel& proc,
    const Process::Cable& cbl);

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
    const Process::ProcessModel& parentProcess, const Process::Outlet& p,
    bool tryOtherOutlets = false);
SCORE_PLUGIN_SCENARIO_EXPORT
void loadPresetAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p,
    bool tryOtherOutlets = false);

}
