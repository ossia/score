#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Midi/MidiProcess.hpp>
#include <Midi/MidiLayer.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiView.hpp>

namespace Midi
{
using ProcessFactory = Process::GenericProcessFactory<
    Midi::ProcessModel,
    Midi::Layer,
    Midi::Presenter,
    Midi::View>;
}
