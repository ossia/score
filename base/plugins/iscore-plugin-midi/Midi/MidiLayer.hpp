#pragma once
#include <Midi/MidiProcess.hpp>
#include <Process/LayerModel.hpp>

namespace Midi
{
using Layer = Process::LayerModel_T<ProcessModel>;
}

LAYER_METADATA(
        ,
        Midi::Layer,
        "248952e7-843c-4ad4-b9a4-36987a87d544",
        "MidiLayer",
        "MidiLayer"
        )
