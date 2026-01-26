#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::RenderPipeline
{
class Model;
}

PROCESS_METADATA(
    , Gfx::RenderPipeline::Model, "dbfc2101-40d7-4807-8804-571e88992e7e",
    "RenderPipeline",                                 // Internal name
    "Render Pipeline",                                // Pretty name
    Process::ProcessCategory::Visual,                 // Category
    "Visuals/Render",                                 // Category
    "Generic shader-based render pipeline",           // Description
    "ossia team",                                     // Author
    (QStringList{"gfx", "model", "3d", "rendering"}), // Tags
    {},                                               // Inputs
    {},                                               // Outputs
    QUrl{},                                           // Doc url
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface
        | Process::ProcessFlags::ScriptEditingSupported // Flags
)
