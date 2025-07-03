#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::GeometryFilter
{
class Model;
}

PROCESS_METADATA(
    , Gfx::GeometryFilter::Model, "27d3cc85-a4b0-4924-8fde-71c337b40f59",
    "geomfilter",                         // Internal name
    "Geometry filter",                    // Pretty name
    Process::ProcessCategory::Visual,     // Category
    "Visuals",                            // Category
    "Process geometry in vertex shaders", // Description
    "ossia team",                         // Author
    (QStringList{"shader", "gfx"}),       // Tags
    {},                                   // Inputs
    {},                                   // Outputs
    QUrl(""),
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface
        | Process::ProcessFlags::DynamicPorts // Flags
)
