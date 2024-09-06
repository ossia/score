#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media
{
namespace Merger
{
class Model;
}
}

PROCESS_METADATA(
    , Media::Merger::Model, "fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7", "Merger",
    "Audio Merger", Process::ProcessCategory::Other, "Audio/Utilities",
    "Merge multiple audio inputs into a single audio output", "ossia score", {},
    {std::vector<Process::PortType>{Process::PortType::Audio}},
    {std::vector<Process::PortType>{Process::PortType::Audio}},
    QUrl("https://ossia.io/score-docs/processes/audio-utilities.html#stereo-merger"),
    Process::ProcessFlags::SupportsLasting)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::Merger::Model, "Audio Merger")
