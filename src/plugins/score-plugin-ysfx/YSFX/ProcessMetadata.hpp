#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

namespace YSFX
{
class ProcessModel;
}

PROCESS_METADATA(
    , YSFX::ProcessModel, "cb6b50d3-82c0-4356-a282-317da8b8022f", "JSFX", "JSFX",
    Process::ProcessCategory::AudioEffect, "Plugins",
    "JSFX code, thanks to jpcima's ysfx library.", "ossia score, Jean Pierre Cimalando",
    (QStringList{"Script", "YSFX"}), {}, {},
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::PutInNewSlot
        | Process::ProcessFlags::ControlSurface)
