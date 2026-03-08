#pragma once
#include <Process/ProcessMetadata.hpp>

namespace ClipLauncher
{
class ProcessModel;
}

PROCESS_METADATA(
    , ClipLauncher::ProcessModel, "a8e5e9f0-1b3c-4d7e-9f2a-6c8b4d3e5f7a",
    "ClipLauncher",                      // ObjectKey
    "Clip Launcher",                     // PrettyName
    Process::ProcessCategory::Structure, // Category
    "Structure",                         // Category text
    "Grid-based clip launcher for live performance with lanes, scenes, "
    "and transition rules",                                       // Description
    "ossia score",                                                // Author
    (QStringList{"live", "performance", "clips", "launcher"}),    // Tags
    {},                                                           // Inputs
    {},                                                           // Outputs
    QUrl(""),                                                     // Documentation
    Process::ProcessFlags::SupportsTemporal                       //
        | Process::ProcessFlags::PutInNewSlot                     //
        | Process::ProcessFlags::FullyCustomItem)
