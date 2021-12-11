#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

namespace YSFX
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    YSFX::ProcessModel,
    "cb6b50d3-82c0-4356-a282-317da8b8022f",
    "JSFX",
    "JSFX",
    Process::ProcessCategory::Script,
    "Script",
    "JSFX code, thanks to jpcima's ysfx library.",
    "ossia score, jpcima",
    (QStringList{"Script", "YSFX"}),
    {},
    {},
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::PutInNewSlot | Process::ProcessFlags::ControlSurface)
