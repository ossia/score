#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Video
{
class Model;
}

PROCESS_METADATA(
    ,
    Gfx::Video::Model,
    "32dc5341-7748-4c31-a226-82e6bd685744",
    "video",                           // Internal name
    "Video",                           // Pretty name
    Process::ProcessCategory::Visual,  // Category
    "GFX",                             // Category
    "Display a video",                 // Description
    "ossia team",                      // Author
    (QStringList{"gfx", "video"}),     // Tags
    {},                                // Inputs
    {},                                // Outputs
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot | Process::ProcessFlags::HandlesLooping // Flags
)
