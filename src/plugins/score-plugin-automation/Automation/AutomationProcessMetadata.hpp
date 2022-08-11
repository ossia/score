#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

#include <score_plugin_automation_export.h>

namespace Automation
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_AUTOMATION_EXPORT, Automation::ProcessModel,
    "d2a67bd8-5d3f-404e-b6e9-e350cf2a833f", "Automation", "Automation (float)",
    Process::ProcessCategory::Automation, "Automations", "Automation on a single value",
    "ossia score", (QStringList{"Curve", "Automation"}), {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
