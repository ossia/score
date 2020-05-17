#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score_plugin_midi_export.h>

namespace Midi
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_MIDI_EXPORT,
    Midi::ProcessModel,
    "c189e507-3afa-4a53-9369-e38860e6bb2d",
    "Midi",
    "Piano roll",
    Process::ProcessCategory::MediaSource,
    "Midi",
    "MIDI note playback",
    "ossia score",
    {},
    {},
    {std::vector<Process::PortType>{Process::PortType::Midi}},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)
