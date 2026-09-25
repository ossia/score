#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::Sink
{
class Model;
}

PROCESS_METADATA(
    , Gfx::Sink::Model, "ba99c123-1379-4caf-b497-0fa5e2b953bb",
    "gfx_sink",                       // Internal name
    "Sink",                           // Pretty name
    Process::ProcessCategory::Visual, // Category
    "Visuals/Utilities",              // Category
    "Runs the video processes connected to it at a set rate, without "
    "showing anything: for a process whose result is data, such as an AI "
    "model's Data outlet, when its image goes to no output", // Description
    "ossia team",                                            // Author
    (QStringList{"gfx", "sink", "output"}),                  // Tags
    {},                                                      // Inputs
    {},                                                      // Outputs
    QUrl{},
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::ControlSurface // Flags
)
