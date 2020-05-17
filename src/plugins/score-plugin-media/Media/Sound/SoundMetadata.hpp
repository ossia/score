#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media
{
namespace Sound
{
class ProcessModel;
}
}

PROCESS_METADATA(
    ,
    Media::Sound::ProcessModel,
    "63174570-d608-44bf-a9cb-e6f5a11f73cc",
    "Sound",
    "Sound file",
    Process::ProcessCategory::MediaSource,
    "Audio",
    "Reads a sound file",
    "ossia score",
    {},
    {},
    {std::vector<Process::PortType>{Process::PortType::Audio}},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::Sound::ProcessModel, "Sound")
