#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::BufferGeometry
{
class Model;
}

PROCESS_METADATA(
    , Gfx::BufferGeometry::Model, "61ad3eae-8447-4197-9533-4773da4e9dd2",
    "buffer-geometry",                                                 // Internal name
    "Buffer Geometry",                                                 // Pretty name
    Process::ProcessCategory::Visual,                                  // Category
    "Visuals/3D",                                                      // Category
    "Convert buffers to geometry with configurable vertex attributes", // Description
    "ossia team",                                                      // Author
    (QStringList{"gfx", "buffer", "geometry", "vertex"}),              // Tags
    {},                                                                // Inputs
    {},                                                                // Outputs
    QUrl(""),
    Process::ProcessFlags::SupportsAll // Flags
)
