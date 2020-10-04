#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Images
{
class Model;
}

PROCESS_METADATA(
    ,
    Gfx::Images::Model,
    "e96c5c0b-7e09-49fb-a851-ff6f4811bb00",
    "images",                          // Internal name
    "Images",                          // Pretty name
    Process::ProcessCategory::Visual,  // Category
    "GFX",                             // Category
    "Display a set of images",         // Description
    "ossia team",                      // Author
    (QStringList{"gfx", "image"}),     // Tags
    {},                                // Inputs
    {},                                // Outputs
    Process::ProcessFlags::SupportsAll // Flags
)
