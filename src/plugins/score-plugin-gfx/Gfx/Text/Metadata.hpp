#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Text
{
class Model;
}

PROCESS_METADATA(
    , Gfx::Text::Model, "88bd9718-2a36-42ba-8eab-da5f84e3978e",
    "text",                           // Internal name
    "Text",                           // Pretty name
    Process::ProcessCategory::Visual, // Category
    "Visuals/Textures",               // Category
    "Display text",                   // Description
    "ossia team",                     // Author
    (QStringList{"gfx", "text"}),     // Tags
    {std::vector<Process::PortType>{Process::PortType::Message}}, // Inputs
    {std::vector<Process::PortType>{Process::PortType::Texture}}, // Outputs
    QUrl("https://ossia.io/score-docs/processes/text.html"),
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface // Flags
)
