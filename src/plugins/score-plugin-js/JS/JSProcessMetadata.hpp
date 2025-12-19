#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

namespace JS
{
class ProcessModel;
}

PROCESS_METADATA(
    , JS::ProcessModel, "846a5de5-47f9-46c5-a898-013cb20951d0", "Javascript",
    "Javascript", Process::ProcessCategory::Script, "Script",
    "Javascript code. See the documentation at https://ossia.io.", "ossia score",
    (QStringList{"Script", "JS"}), {}, {},
    QUrl("https://ossia.io/score-docs/processes/javascript.html#javascript-support"),
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::PutInNewSlot
        | Process::ProcessFlags::ControlSurface | Process::ProcessFlags::DynamicPorts | Process::ProcessFlags::ScriptEditingSupported)
