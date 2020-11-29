#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

#include <score_plugin_automation_export.h>

namespace Gradient
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_AUTOMATION_EXPORT,
    Gradient::ProcessModel,
    "b5da735b-a76d-4314-8853-3e8a96486fb9",
    "Gradient",
    "Color automation",
    Process::ProcessCategory::Automation,
    "Automations",
    "Color gradient. Operates in Lab color space.",
    "ossia score",
    (QStringList{"Curve", "Automation", "Color"}),
    {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
