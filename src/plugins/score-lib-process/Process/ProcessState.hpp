#pragma once
#include <QByteArray>

#include <score_lib_process_export.h>

namespace Process
{
class ProcessModel;

//! Whether a cue can recall what the process holds beyond its controls;
//! false for a process laid out in time, whose content is not a setting.
SCORE_LIB_PROCESS_EXPORT bool recallsState(const ProcessModel& proc) noexcept;

//! The state of a process beyond its controls (script, shader...) as JSON;
//! empty when there is none or a cue cannot recall it.
SCORE_LIB_PROCESS_EXPORT QByteArray stateBeyondControls(const ProcessModel& proc);

//! Restores such a state as an undoable edit, unless the program is unchanged.
SCORE_LIB_PROCESS_EXPORT void
applyStateBeyondControls(ProcessModel& proc, const QByteArray& state);
}
