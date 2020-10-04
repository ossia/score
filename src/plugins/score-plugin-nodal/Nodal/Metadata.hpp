#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Nodal
{
class Model;
}

PROCESS_METADATA(
    ,
    Nodal::Model,
    "f5678806-c431-45c5-ae3a-fae5183380fb",
    "Nodal",                                     // Internal name
    "Nodal",                                     // Pretty name
    Process::ProcessCategory::Structure,         // Category
    "Structure",                                 // Category
    "Organise processes in a node-based editor", // Description
    "ossia score",                               // Author
    (QStringList{}),                             // Tags
    {},                                          // Inputs
    {},                                          // Outputs
    Process::ProcessFlags::SupportsAll           // Flags
)
