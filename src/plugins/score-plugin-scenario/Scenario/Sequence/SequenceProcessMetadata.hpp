#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score_plugin_scenario_export.h>

namespace Sequence
{
class SequenceModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_SCENARIO_EXPORT, Sequence::SequenceModel,
    "d4a12a5e-4c3b-4e8a-9f1e-2b7c6d3a4e5f", "Sequence", "Sequence",
    Process::ProcessCategory::Structure, "Structure",
    "Linear multi-section sequence with intermediary states",
    "ossia score", {}, {}, {},
    QUrl("https://ossia.io/score-docs/processes/sequence.html"),
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)
