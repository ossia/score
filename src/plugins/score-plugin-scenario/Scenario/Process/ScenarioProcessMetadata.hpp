#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_SCENARIO_EXPORT,
    Scenario::ProcessModel,
    "de035912-5b03-49a8-bc4d-b2cba68e21d9",
    "Scenario",
    "Scenario",
    Process::ProcessCategory::Structure,
    "Structure",
    "Temporal structure",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)
