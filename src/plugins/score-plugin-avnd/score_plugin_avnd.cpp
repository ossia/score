/*
#include <Examples/AudioEffect.hpp>
#include <Examples/AudioEffectWithSidechains.hpp>
#include <Examples/CCC.hpp>
#include <Examples/ControlGallery.hpp>
#include <Examples/Distortion.hpp>
#include <Examples/Empty.hpp>
#include <Examples/RawPorts.hpp>
#include <Examples/SampleAccurateFilter.hpp>
#include <Examples/SampleAccurateGenerator.hpp>
#include <Examples/Synth.hpp>
#include <Examples/TextureFilter.hpp>
#include <Examples/TextureGenerator.hpp>
#include <Examples/TrivialFilter.hpp>
#include <Examples/TrivialGenerator.hpp>
#include <Examples/ZeroDependencyAudioEffect.hpp>

*/
#include "score_plugin_avnd.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <ossia/detail/typelist.hpp>

#include <boost/pfr.hpp>

#include <halp/log.hpp>

#include <score_plugin_engine.hpp>

#include <Aether/src/aether_dsp.cpp>

#include <avnd/../../examples/Advanced/Utilities/ADSR.hpp>
#include <avnd/../../examples/Advanced/Utilities/AudioFilters.hpp>
#include <avnd/../../examples/Advanced/Utilities/Bitcrush.hpp>
#if __has_include(<kfr/dft/convolution.hpp>)
#include <avnd/../../examples/Advanced/Utilities/Convolver.hpp>
#endif
#include <avnd/../../examples/Advanced/Utilities/Dynamics.hpp>
#include <avnd/../../examples/Advanced/Utilities/Echo.hpp>
#include <avnd/../../examples/Advanced/Utilities/Flanger.hpp>
#include <avnd/../../examples/Advanced/Utilities/StereoMixer.hpp>
#define AVND_TEST_BUILD 0
#if AVND_TEST_BUILD
#include <avnd/../../examples/Helpers/Controls.hpp>
#include <avnd/../../examples/Helpers/FFTDisplay.hpp>
#include <avnd/../../examples/Helpers/ImageUi.hpp>
#include <avnd/../../examples/Helpers/Logger.hpp>
#include <avnd/../../examples/Helpers/Lowpass.hpp>
#include <avnd/../../examples/Helpers/Messages.hpp>
#include <avnd/../../examples/Helpers/Midi.hpp>
#include <avnd/../../examples/Helpers/Noise.hpp>
#include <avnd/../../examples/Helpers/Peak.hpp>
#include <avnd/../../examples/Helpers/PerBus.hpp>
#include <avnd/../../examples/Helpers/PerSample.hpp>
#include <avnd/../../examples/Helpers/Sines.hpp>
#include <avnd/../../examples/Helpers/Ui.hpp>
#include <avnd/../../examples/Helpers/UiBus.hpp>
#include <avnd/../../examples/Raw/Addition.hpp>
#include <avnd/../../examples/Raw/Callback.hpp>
#include <avnd/../../examples/Raw/Init.hpp>
#include <avnd/../../examples/Raw/Lowpass.hpp>
#include <avnd/../../examples/Raw/Messages.hpp>
#include <avnd/../../examples/Raw/Midi.hpp>
#include <avnd/../../examples/Raw/Minimal.hpp>
#include <avnd/../../examples/Raw/Modular.hpp>
#include <avnd/../../examples/Raw/PerSampleProcessor.hpp>
#include <avnd/../../examples/Raw/PerSampleProcessor2.hpp>
#include <avnd/../../examples/Raw/Presets.hpp>
#include <avnd/../../examples/Raw/SampleAccurateControls.hpp>
#include <avnd/../../examples/Raw/Sines.hpp>
#include <avnd/../../examples/Raw/Ui.hpp>
#include <avnd/../../examples/Tutorial/AudioEffectExample.hpp>
#include <avnd/../../examples/Tutorial/TextureFilterExample.hpp>
#include <avnd/../../examples/Tutorial/TrivialFilterExample.hpp>
#include <avnd/../../examples/Tutorial/TrivialGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/ZeroDependencyAudioEffect.hpp>
// #include <avnd/../../examples/Tutorial/Synth.hpp>
#include <avnd/../../examples/Gpu/Compute.hpp>
#include <avnd/../../examples/Gpu/DrawRaw.hpp>
#include <avnd/../../examples/Gpu/DrawWithHelpers.hpp>
#include <avnd/../../examples/Gpu/SolidColor.hpp>
#include <avnd/../../examples/Ports/LitterPower/CCC.hpp>
#include <avnd/../../examples/Tutorial/AudioSidechainExample.hpp>
#include <avnd/../../examples/Tutorial/ControlGallery.hpp>
#include <avnd/../../examples/Tutorial/Distortion.hpp>
#include <avnd/../../examples/Tutorial/EmptyExample.hpp>
#include <avnd/../../examples/Tutorial/SampleAccurateFilterExample.hpp>
#include <avnd/../../examples/Tutorial/SampleAccurateGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/TextureGeneratorExample.hpp>
#endif
#include <brigand/sequences/list.hpp>

#include <avnd/../../examples/Advanced/Granular/Granolette.hpp>
#include <avnd/../../examples/Helpers/PeakBandFFTPort.hpp>

/**
 * This file instantiates the classes that are provided by this plug-in.
 */

#include <Avnd/Factories.hpp>
#include <Avnd/Logger.hpp>
#include <halp/meta.hpp>

namespace grph
{
class Graph;
}

score_plugin_avnd::score_plugin_avnd() = default;
score_plugin_avnd::~score_plugin_avnd() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_avnd::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  using namespace oscr;
  struct config
  {
    using logger_type = oscr::Logger;
  };
  auto fx = oscr::instantiate_fx<
      Aether::Object, ao::ADSR
#if __has_include(<kfr/dft/convolution.hpp>)
      ,
      ao::Convolver
#endif
      ,
      ao::Lowpass, ao::Highpass, ao::Lowshelf, ao::Highshelf, ao::Bandpass, ao::Bandstop,
      ao::Bandshelf, ao::Bitcrush, ao::Compressor, ao::Limiter, ao::Echo, ao::Flanger,
      ao::StereoMixer>(ctx, key);

  custom_factories<grph::Graph>(fx, ctx, key);

#if AVND_TEST_BUILD
  namespace E = examples;
  namespace EH = examples::helpers;
  auto debug_fx = oscr::instantiate_fx<
      oscr::Granolette, EH::FFTDisplay, EH::MessageBusUi, EH::AdvancedUi, E::Addition,
      E::Callback, EH::Controls, EH::Logger<config>, E::Lowpass, EH::Lowpass,
      E::Messages, EH::Messages<config>, EH::Midi, EH::WhiteNoise, EH::Peak,
      EH::PerBusAsArgs, EH::PerBusAsPortsFixed, EH::PerBusAsPortsDynamic,
      EH::PerSampleAsArgs, EH::PerSampleAsPorts, EH::Sine, E::Init,
      litterpower_ports::CCC, E::Midi, E::Minimal, E::Modular, E::PerSampleProcessor,
      E::PerSampleProcessor2, E::Presets, E::SampleAccurateControls, E::Sine, E::Ui,
      E::Distortion, E::AudioEffectExample, E::AudioSidechainExample, E::EmptyExample,
      E::SampleAccurateGeneratorExample, E::SampleAccurateFilterExample,
      E::ControlGallery
//      , RawPortsExample
//      , Synth
#if SCORE_PLUGIN_GFX
      ,
      E::TextureGeneratorExample, E::TextureFilterExample, E::GpuComputeExample,
      E::GpuFilterExample, E::GpuRawExample, E::GpuSolidColorExample
#endif
      ,
      E::TrivialGeneratorExample, E::TrivialFilterExample, E::ZeroDependencyAudioEffect>(
      ctx, key);
  fx.insert(
      fx.end(), std::make_move_iterator(debug_fx.begin()),
      std::make_move_iterator(debug_fx.end()));
#endif
  return fx;
}
std::vector<score::PluginKey> score_plugin_avnd::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_avnd)
