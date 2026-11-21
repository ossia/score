#pragma once
#include <Process/ProcessFlags.hpp>

#include <QPointF>

#include <Library/ProcessPicker.hpp>
#include <score_plugin_scenario_export.h>

#include <functional>
#include <optional>

namespace score
{
struct GUIApplicationContext;
struct DocumentContext;
}
namespace Process
{
class ProcessModel;
class Port;
}
namespace Scenario
{
class IntervalModel;
class ProcessModel;
struct Point;
namespace Command
{
class Macro;
}

//! Opens the process picker at the mouse, for the processes with one of the
//! `acceptable` flags: `picked` gets the choice, if one is made.
SCORE_PLUGIN_SCENARIO_EXPORT
void pickProcess(
    const score::GUIApplicationContext& ctx, Process::ProcessFlags acceptable,
    std::function<void(const Library::PickedProcess&)> picked,
    Library::PickerOptions options = {});

//! Adds the picked process to `interval` in a new slot, with its preset if one
//! was picked; its node goes at `pos` in the node view, or where a new one
//! goes when none is given (newProcessPosition).
SCORE_PLUGIN_SCENARIO_EXPORT
Process::ProcessModel* addPickedProcess(
    Command::Macro& m, const IntervalModel& interval, const Library::PickedProcess& p,
    std::optional<QPointF> pos = {});

//! Makes a box starting at `pt` in `scenario`, with the picked process in it.
//! It starts when the score plays, as a state made by double-clicking does.
SCORE_PLUGIN_SCENARIO_EXPORT
const IntervalModel* addPickedProcessInNewBox(
    const score::DocumentContext& ctx, const Scenario::ProcessModel& scenario,
    const Scenario::Point& pt, const Library::PickedProcess& p);

//! What a cable from `port` can go to in the picker: a port of its type, of
//! the other direction.
SCORE_PLUGIN_SCENARIO_EXPORT
Library::PortFilter portFilter(const Process::Port& port);
}
