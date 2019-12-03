#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

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
    "(Experimental) Frequency curve",
    Process::ProcessCategory::Automation,
    "Automations",
    "(Experimental) Sends messages at the speed given by the curve",
    "ossia score",
    (QStringList{"Curve", "Automation"}),
    {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
