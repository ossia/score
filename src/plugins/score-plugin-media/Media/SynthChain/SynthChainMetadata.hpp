#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media::SynthChain
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    Media::SynthChain::ProcessModel,
    "f9e07da4-c4dd-4118-b93f-57d827ad83ef",
    "Effects",
    "Synth chain",
    Process::ProcessCategory::Structure,
    "Structure",
    "Put a synth in a chain with MIDI and audio processes",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot
        | Process::ProcessFlags::TimeIndependent)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::SynthChain::ProcessModel, "Effects")
