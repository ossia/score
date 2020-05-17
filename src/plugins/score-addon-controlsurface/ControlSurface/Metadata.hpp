#pragma once
#include <Process/ProcessMetadata.hpp>

namespace ControlSurface
{
class Model;
}

PROCESS_METADATA(
    ,
    ControlSurface::Model,
    "3e9672a5-e005-4392-b062-2b0b3b256d54",
    "ControlSurface",                    // Internal name
    "Control surface",                   // Pretty name
    Process::ProcessCategory::Structure, // Category
    "Control",                           // Category
    "Control external parameters by dropping them from the device tree in "
    "this process",                    // Description
    "ossia score",                     // Author
    (QStringList{}),                   // Tags
    {},                                // Inputs
    {},                                // Outputs
    Process::ProcessFlags::SupportsAll // Flags
)
