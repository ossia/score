#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

namespace Gfx::CSF
{
class Model;
}

PROCESS_METADATA(
    , Gfx::CSF::Model, "a5bbffe0-93d2-4e70-995c-cf46c2c43520",
    "CSF",            // Internal name
    "Compute Shader", // Pretty name
    Process::ProcessCategory::Visual,
    "Visuals",                               // Category
    "Compute shaders",                       // Description
    "ossia team",                            // Author
    (QStringList{"gfx", "compute", "glsl"}), // Tags
    {},                                      // Inputs
    {},                                      // Outputs
    QUrl{},                                  // Documentation link
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface
        | Process::ProcessFlags::PutInNewSlot)
