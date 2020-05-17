#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media::AudioChain
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    Media::AudioChain::ProcessModel,
    "63f54652-96b7-4cbc-b2c1-d6aa1a6cb341",
    "Effects",
    "Audio effect chain",
    Process::ProcessCategory::Structure,
    "Structure",
    "Puts audio processes one after the other",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot
        | Process::ProcessFlags::TimeIndependent)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::AudioChain::ProcessModel, "Effects")
