#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media
{
namespace Metro
{
class Model;
}
}

PROCESS_METADATA(
    ,
    Media::Metro::Model,
    "7a20cee3-87da-42a5-9483-975aa33a7a65",
    "Metro",
    "Metronome",
    Process::ProcessCategory::Generator,
    "Audio",
    "Generates sound according to the current beat",
    "ossia score",
    {},
    {},
    {std::vector<Process::PortType>{Process::PortType::Audio}},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::Metro::Model, "Metronom")
