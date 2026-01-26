#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::ModelDisplay
{
class Model;
}

PROCESS_METADATA(
    , Gfx::ModelDisplay::Model, "9ce44e4b-eeb6-4042-bb7f-9d0b28190daf",
    "modeldisplay",                      // Internal name
    "Model Display",                     // Pretty name
    Process::ProcessCategory::Visual,    // Category
    "Visuals/Render",                    // Category
    "Display input geometry",            // Description
    "ossia team",                        // Author
    (QStringList{"gfx", "model", "3d"}), // Tags
    {},                                  // Inputs
    {},                                  // Outputs
    QUrl{},                              // Doc url
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface // Flags
)
