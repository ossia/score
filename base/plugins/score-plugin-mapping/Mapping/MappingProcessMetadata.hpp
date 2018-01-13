#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score_plugin_mapping_export.h>

namespace Mapping
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_MAPPING_EXPORT,
    Mapping::ProcessModel,
    "12a5d9b8-823e-4303-99f8-34db37c448b4",
    "Mapping",
    "Mapping (float)",
    "Mappings",
    (QStringList{"Curve", "Mapping"}),
    Process::ProcessFlags::SupportsTemporal)
