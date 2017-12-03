#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score/plugins/customfactory/UuidKey.hpp>
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
    "Automation (color)",
    "Automations",
    (QStringList{"Curve", "Automation", "Color"}))
