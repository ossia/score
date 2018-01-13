#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score_plugin_automation_export.h>

namespace Automation
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_AUTOMATION_EXPORT,
    Automation::ProcessModel,
    "d2a67bd8-5d3f-404e-b6e9-e350cf2a833f",
    "Automation",
    "Automation (float)",
    "Automations",
    (QStringList{"Curve", "Automation"}),
    Process::ProcessFlags::SupportsTemporal)
