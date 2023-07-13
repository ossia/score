#include <score/plugins/FactorySetup.hpp>

#define AVND_TEST_BUILD 0
#if AVND_TEST_BUILD
#include <ossia/detail/logger.hpp>

#include <Avnd/Factories.hpp>
#include <Avnd/Logger.hpp>
#include <halp/log.hpp>

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
#include <avnd/../../examples/Raw/SpanControls.hpp>
#include <avnd/../../examples/Raw/Ui.hpp>
#include <avnd/../../examples/Tutorial/AudioEffectExample.hpp>
#include <avnd/../../examples/Tutorial/TextureFilterExample.hpp>
#include <avnd/../../examples/Tutorial/TrivialFilterExample.hpp>
#include <avnd/../../examples/Tutorial/TrivialGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/ZeroDependencyAudioEffect.hpp>
// #include <avnd/../../examples/Tutorial/Synth.hpp>

#include <avnd/../../examples/Advanced/Granular/Granolette.hpp>
#include <avnd/../../examples/Gpu/DrawRaw.hpp>
#include <avnd/../../examples/Gpu/DrawWithHelpers.hpp>
#include <avnd/../../examples/Helpers/PeakBandFFTPort.hpp>
#include <avnd/../../examples/Ports/LitterPower/CCC.hpp>
#include <avnd/../../examples/Tutorial/AudioSidechainExample.hpp>
#include <avnd/../../examples/Tutorial/ControlGallery.hpp>
#include <avnd/../../examples/Tutorial/Distortion.hpp>
#include <avnd/../../examples/Tutorial/EmptyExample.hpp>
#include <avnd/../../examples/Tutorial/SampleAccurateFilterExample.hpp>
#include <avnd/../../examples/Tutorial/SampleAccurateGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/TextureGeneratorExample.hpp>

namespace oscr
{
void instantiate_tests(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  struct config
  {
    using logger_type = oscr::Logger;
  };
  namespace E = examples;
  namespace EH = examples::helpers;

  oscr::instantiate_fx<examples::SpanControls>(fx, ctx, key);

  oscr::instantiate_fx<
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
      E::TextureGeneratorExample, E::TextureFilterExample, E::GpuFilterExample,
      E::GpuRawExample
#endif
      ,
      E::TrivialGeneratorExample, E::TrivialFilterExample, E::ZeroDependencyAudioEffect>(
      fx, ctx, key);
}
}
#else
namespace oscr
{
void instantiate_tests(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
}
}
#endif
