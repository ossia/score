#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Mesh
{
class Model;
}

PROCESS_METADATA(
    ,
    Gfx::Mesh::Model,
    "faf53337-c28a-47ed-896a-f68c9d5f5601",
    "mesh",                    // Internal name
    "Mesh",                   // Pretty name
    Process::ProcessCategory::Visual,  // Category
    "GFX",                             // Category
    "Mesh",                   // Description
    "ossia team",                      // Author
    (QStringList{"shader", "gfx"}),    // Tags
    {},                                // Inputs
    {},                                // Outputs
    Process::ProcessFlags::SupportsAll // Flags
)
