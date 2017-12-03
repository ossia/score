#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score_plugin_automation_export.h>

namespace Metronome
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_AUTOMATION_EXPORT,
    Metronome::ProcessModel,
    "e6f5d1fd-6b32-4799-ba53-ff793b3faabc",
    "Metronome",
    "Metronome",
    "Automations",
    (QStringList{"Curve", "Automation"}))
