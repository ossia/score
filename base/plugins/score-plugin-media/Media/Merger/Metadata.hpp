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
    ,
    Media::Merger::Model,
    "fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7",
    "Merger",
    "Stereo Merger",
    "Audio",
    {},
    Process::ProcessFlags::SupportsTemporal
        | Process::ProcessFlags::SupportsEffectChain)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::Merger::Model, "Stereo Merger")
