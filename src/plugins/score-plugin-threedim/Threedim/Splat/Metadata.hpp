#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Splat
{
class Model;
}

PROCESS_METADATA(
    , Gfx::Splat::Model, "cdc15a16-e856-4e02-9339-7d9e48da10ce",
    "Splat",                             // Internal name
    "Splat",                             // Pretty name
    Process::ProcessCategory::Visual,    // Category
    "Visuals/Render",                    // Category
    "Display gaussian splats",           // Description
    "ossia team",                        // Author
    (QStringList{"gfx", "model", "3d"}), // Tags
    {},                                  // Inputs
    {},                                  // Outputs
    QUrl{},                              // Doc url
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface // Flags
)
