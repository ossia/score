#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

namespace Gfx::VSA
{
class Model;
}

PROCESS_METADATA(
    , Gfx::VSA::Model, "ea13ed06-d21c-4c84-8d0f-83ce0027b81c",
    "vsa",                                                // Internal name
    "Vertex Shader Art",                                  // Pretty name
    Process::ProcessCategory::Visual,                     // Category
    "Visuals",                                            // Category
    "Vertex Shader Art. See https://vertexshaderart.com", // Description
    "ossia score",                                        // Author
    (QStringList{"vsa", "vertex", "shader", "gfx"}),      // Tags
    {},                                                   // Inputs
    {},                                                   // Outputs
    QUrl("https://vertexshaderart.com"),
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface
        | Process::ProcessFlags::DynamicPorts | Process::ProcessFlags::ScriptEditingSupported // Flags
)
