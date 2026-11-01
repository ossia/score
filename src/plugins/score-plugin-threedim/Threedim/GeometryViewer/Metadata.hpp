#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::GeometryViewer
{
class Model;
}

PROCESS_METADATA(
    , Gfx::GeometryViewer::Model, "e8e99740-31b8-4392-8098-4255759d38a6",
    "geometryviewer",                     // Internal name
    "Geometry viewer",                    // Pretty name
    Process::ProcessCategory::Visual,     // Category
    "Monitoring/Visuals",                 // Category
    "Look at a geometry from an orbiting camera: shaded, wireframe, points, "
    "vertex colours or texture coordinates", // Description
    "ossia team",                         // Author
    (QStringList{"gfx", "geometry", "3d", "monitoring"}), // Tags
    {},                                   // Inputs
    {},                                   // Outputs
    QUrl{},                               // Doc url
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface
        | Process::ProcessFlags::FullyCustomItem // Flags
)
