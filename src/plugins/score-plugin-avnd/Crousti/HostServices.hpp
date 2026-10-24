#pragma once
#include <halp/custom_widgets.hpp>

#include <score_plugin_avnd_export.h>

namespace Process
{
class ProcessModel;
}
namespace score
{
struct DocumentContext;
}

namespace oscr
{
//! Places a file in the timeline the way dropping it would, as a new box of
//! the scenario that holds `proc`, starting where the clip says it was made
//! and below the box of `proc`, reached from the scenario's start so that it
//! plays at that date. One undoable command. Does nothing, and says
//! why in the log, when the process is not inside a scenario or nothing knows
//! how to make a process of the file.
SCORE_PLUGIN_AVND_EXPORT
void sendToTimeline(
    const Process::ProcessModel& proc, const score::DocumentContext& ctx,
    const halp::timeline_clip& clip);
}
