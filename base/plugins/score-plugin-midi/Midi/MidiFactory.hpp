#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace Midi
{
using ProcessFactory = Process::ProcessFactory_T<Midi::ProcessModel>;
using LayerFactory = Process::
    LayerFactory_T<Midi::ProcessModel, Midi::Presenter, Midi::View, Process::GraphicsViewLayerPanelProxy>;
}
