#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

#include <score_plugin_mapping_export.h>

namespace Mapping
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_MAPPING_EXPORT, Mapping::ProcessModel,
    "12a5d9b8-823e-4303-99f8-34db37c448b4", "Mapping", "Mapping curve",
    Process::ProcessCategory::Mapping, "Control/Mappings", "Mapping on a single value",
    "ossia score", (QStringList{"Curve", "Mapping"}),
    {std::vector<Process::PortType>{Process::PortType::Message}},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    QUrl("https://ossia.io/score-docs/processes/2Dspline.html#generating-a-curve"),
    Process::ProcessFlags::SupportsLasting
        | Process::ProcessFlags::ItemRequiresUniqueFocus)
