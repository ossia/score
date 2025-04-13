#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score_plugin_midi_export.h>

namespace Patternist
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_MIDI_EXPORT, Patternist::ProcessModel,
    "49047204-5c1e-4b54-9f43-2b583f664b2e", "Pattern", "Pattern sequencer",
    Process::ProcessCategory::MediaSource, "Midi", "", "Drum patterns", {}, {},
    {std::vector<Process::PortType>{Process::PortType::Midi}},
    QUrl("https://ossia.io/score-docs/processes/patternist.html#midi-pattern-sequencer"),
    Process::ProcessFlags::SupportsLasting | Process::ProcessFlags::PutInNewSlot)
