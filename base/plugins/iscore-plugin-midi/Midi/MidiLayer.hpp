#pragma once
#include <Midi/MidiProcess.hpp>
#include <Process/LayerModel.hpp>

namespace Midi
{
using Layer = Process::LayerModel_T<ProcessModel>;
}

DEFAULT_MODEL_METADATA(Midi::Layer, "MidiLayer")
