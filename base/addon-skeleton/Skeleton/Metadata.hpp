#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Skeleton
{
class Model;
}

PROCESS_METADATA(
    , Skeleton::Model, "00000000-0000-0000-0000-000000000000",
    "Skeleton",                                   // Internal name
    "Skeleton",                                   // Pretty name
    "Other",                                      // Category
    (QStringList{"Put", "Your", "Tags", "Here"}), // Tags
    Process::ProcessFlags::SupportsAll            // Flags
)
