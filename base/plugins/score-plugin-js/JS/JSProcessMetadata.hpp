#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>

namespace JS
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    JS::ProcessModel,
    "846a5de5-47f9-46c5-a898-013cb20951d0",
    "Javascript",
    "Javascript",
    "Script",
    {},
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::PutInNewSlot)
