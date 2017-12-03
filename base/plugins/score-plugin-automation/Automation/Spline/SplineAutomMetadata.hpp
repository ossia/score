#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score_plugin_automation_export.h>

namespace Spline
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_AUTOMATION_EXPORT,
    Spline::ProcessModel,
    "931a6356-2fca-4f3b-9c65-2de051ef4903",
    "Spline",
    "Automation (XY)",
    "Automations",
    (QStringList{"Curve", "Automation", "2D"}))
