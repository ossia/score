#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Filter
{
class Model;
}

PROCESS_METADATA(
    ,
    Gfx::Filter::Model,
    "74ca45ff-92c9-44a0-8f1a-754dea05ee1b",
    "shaderfilter",                    // Internal name
    "Shader filter",                   // Pretty name
    Process::ProcessCategory::Visual,  // Category
    "GFX",                             // Category
    "Shader filter",                   // Description
    "ossia team",                      // Author
    (QStringList{"shader", "gfx"}),    // Tags
    {},                                // Inputs
    {},                                // Outputs
    Process::ProcessFlags::SupportsAll // Flags
)
