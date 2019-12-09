#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media
{
namespace Effect
{
class ProcessModel;
}
}

PROCESS_METADATA(
    ,
    Media::Effect::ProcessModel,
    "d27bc0ed-a93e-434c-913d-ccab0b22b4e8",
    "Effects",
    "Audio effect chain",
    Process::ProcessCategory::Structure,
    "Structure",
    "Puts audio processes one after the other",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::SupportsTemporal
        | Process::ProcessFlags::PutInNewSlot
        | Process::ProcessFlags::TimeIndependent)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::Effect::ProcessModel, "Effects")
