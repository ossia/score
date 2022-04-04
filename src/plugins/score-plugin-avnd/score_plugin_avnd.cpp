/*
#include <Examples/AudioEffect.hpp>
#include <Examples/AudioEffectWithSidechains.hpp>
#include <Examples/Empty.hpp>
#include <Examples/Distortion.hpp>
#include <Examples/ZeroDependencyAudioEffect.hpp>

#include <Examples/SampleAccurateGenerator.hpp>
#include <Examples/SampleAccurateFilter.hpp>

#include <Examples/TextureGenerator.hpp>
#include <Examples/TextureFilter.hpp>
#include <Examples/TrivialGenerator.hpp>
#include <Examples/TrivialFilter.hpp>
#include <Examples/RawPorts.hpp>
#include <Examples/ControlGallery.hpp>

#include <Examples/CCC.hpp>
#include <Examples/Synth.hpp>

*/
#include "score_plugin_avnd.hpp"

#include <Crousti/ProcessModel.hpp>
#include <Crousti/Executor.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <boost/pfr.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <score_plugin_engine.hpp>
#include <ossia/detail/typelist.hpp>
#include <avnd/../../examples/Raw/Addition.hpp>
#include <avnd/../../examples/Raw/Callback.hpp>
#include <avnd/../../examples/Raw/Init.hpp>
#include <avnd/../../examples/Raw/Presets.hpp>
#include <avnd/../../examples/Raw/PerSampleProcessor2.hpp>
#include <avnd/../../examples/Raw/Lowpass.hpp>
#include <avnd/../../examples/Raw/Ui.hpp>
#include <avnd/../../examples/Raw/Midi.hpp>
#include <avnd/../../examples/Raw/PerSampleProcessor.hpp>
#include <avnd/../../examples/Raw/Modular.hpp>
#include <avnd/../../examples/Raw/SampleAccurateControls.hpp>
#include <avnd/../../examples/Raw/Minimal.hpp>
#include <avnd/../../examples/Raw/Messages.hpp>
#include <avnd/../../examples/Helpers/Controls.hpp>
#include <avnd/../../examples/Helpers/ImageUi.hpp>
#include <avnd/../../examples/Helpers/Logger.hpp>
#include <avnd/../../examples/Helpers/Lowpass.hpp>
#include <avnd/../../examples/Helpers/Messages.hpp>
#include <avnd/../../examples/Helpers/Noise.hpp>
#include <avnd/../../examples/Helpers/PerSample.hpp>
#include <avnd/../../examples/Helpers/PerBus.hpp>
#include <avnd/../../examples/Helpers/Peak.hpp>
#include <avnd/../../examples/Helpers/Midi.hpp>
#include <avnd/../../examples/Helpers/Ui.hpp>
#include <avnd/../../examples/Helpers/UiBus.hpp>
#include <avnd/../../examples/Tutorial/ZeroDependencyAudioEffect.hpp>
#include <avnd/../../examples/Tutorial/AudioEffectExample.hpp>
#include <avnd/../../examples/Tutorial/TextureFilterExample.hpp>
#include <avnd/../../examples/Tutorial/TrivialGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/TrivialFilterExample.hpp>
// #include <avnd/../../examples/Tutorial/Synth.hpp>
#include <avnd/../../examples/Tutorial/TextureGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/EmptyExample.hpp>
#include <avnd/../../examples/Tutorial/SampleAccurateFilterExample.hpp>
#include <avnd/../../examples/Tutorial/ControlGallery.hpp>
#include <avnd/../../examples/Tutorial/AudioSidechainExample.hpp>
#include <avnd/../../examples/Tutorial/SampleAccurateGeneratorExample.hpp>
#include <avnd/../../examples/Tutorial/Distortion.hpp>
#include <avnd/../../examples/Ports/LitterPower/CCC.hpp>
#include <brigand/sequences/list.hpp>


#include <Crousti/Layer.hpp>
/**
 * This file instantiates the classes that are provided by this plug-in.
 */

#include <halp/meta.hpp>

struct MyProcessor {
  halp_meta(name, "Gain");
  halp_meta(uuid, "3183d03e-9228-4d50-98e0-e7601dd16a2e");

  struct ins {
    halp::dynamic_audio_bus<"Input", double> audio;
    halp::knob_f32<"Gain", halp::range{.min = 0., .max = 1.}> gain;
  } inputs;

  struct outs {
    halp::dynamic_audio_bus<"Output", double> audio;
    halp::hbargraph_f32<"Measure", halp::range{-1., 1., 0.}> measure;
  } outputs;

  struct ui {
    using enum halp::colors;
    using enum halp::layouts;

    halp_meta(name, "Main")
    halp_meta(layout, hbox)
    halp_meta(background, mid)

    struct {
      halp_meta(name, "Widget")
      halp_meta(layout, vbox)
      halp_meta(background, dark)

      const char* label = "Hello !";
      halp::item<&ins::gain> widget;
      const char* label2 = "Gain control!";
    } widgets;

    halp::spacing spc{.width = 20, .height = 20};

    halp::item<&outs::measure> widget2;
  };

  void operator()(int N) {
    auto& in = inputs.audio;
    auto& out = outputs.audio;
    const float gain = inputs.gain;

    double measure = 0.;
    for (int i = 0; i < in.channels; i++)
    {
      for (int j = 0; j < N; j++)
      {
        out[i][j] = gain * in[i][j];
        measure += std::abs(out[i][j]);
      }
    }

    if(N > 0 && in.channels > 0)
      outputs.measure = measure / (N * in.channels);
  }
};


namespace oscr
{
template <typename Node>
struct ProcessFactory final
    : public Process::ProcessFactory_T<oscr::ProcessModel<Node>>
{
  using Process::ProcessFactory_T<oscr::ProcessModel<Node>>::ProcessFactory_T;
};

template <typename Node>
struct ExecutorFactory final
    : public Execution::ProcessComponentFactory_T<oscr::Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<oscr::Executor<Node>>::ProcessComponentFactory_T;
};

template <typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>> instantiate_fx(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  std::vector<std::unique_ptr<score::InterfaceBase>> v;

  if (key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    //static_assert((requires { std::declval<Nodes>().run({}, {}); } && ...));
    (v.emplace_back(static_cast<Execution::ProcessComponentFactory*>(new oscr::ExecutorFactory<Nodes>())), ...);
  }
  else if (key == Process::ProcessModelFactory::static_interfaceKey())
  {
    (v.emplace_back(static_cast<Process::ProcessModelFactory*>(new oscr::ProcessFactory<Nodes>())), ...);
  }
  else if (key == Process::LayerFactory::static_interfaceKey())
  {
    ossia::for_each_tagged(brigand::list<Nodes...>{}, [&](auto t) {
      using type = typename decltype(t)::type;
      if constexpr(avnd::has_ui<type>)
      {
        v.emplace_back(static_cast<Process::LayerFactory*>(new oscr::LayerFactory<type>()));
      }
    });
  }

  return v;
}
}

score_plugin_avnd::score_plugin_avnd() = default;
score_plugin_avnd::~score_plugin_avnd() = default;

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_avnd::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace oscr;
  using namespace examples;
  using namespace examples::helpers;

  struct config {
      using logger_type = halp::basic_logger;
  };

  return oscr::instantiate_fx<
      examples::helpers::MessageBusUi
    #if 0
      examples::helpers::AdvancedUi
      , MyProcessor
      , Addition
      , Callback
      , Controls
      , Logger<config>
      , examples::Lowpass
      , examples::helpers::Lowpass
      , examples::Messages
      , examples::helpers::Messages<config>
      , examples::helpers::Midi
      , examples::helpers::WhiteNoise
      , examples::helpers::Peak
      , examples::helpers::PerBusAsArgs
      , examples::helpers::PerBusAsPortsFixed
      , examples::helpers::PerBusAsPortsDynamic
      , examples::helpers::PerSampleAsArgs
      , examples::helpers::PerSampleAsPorts
      , examples::Init
      , litterpower_ports::CCC
      , examples::Midi
      , examples::Minimal
      , examples::Modular
      , examples::PerSampleProcessor
      , examples::PerSampleProcessor2
      , examples::Presets
      , examples::SampleAccurateControls
      , examples::Ui
      , Distortion
      , AudioEffectExample
      , AudioSidechainExample
      , EmptyExample
      , SampleAccurateGeneratorExample
      , SampleAccurateFilterExample
      , ControlGallery
//      , RawPortsExample
//      , Synth
#if SCORE_PLUGIN_GFX
      , TextureGeneratorExample
      , TextureFilterExample
#endif
      , TrivialGeneratorExample
      , TrivialFilterExample
      , ZeroDependencyAudioEffect
    #endif
      >(ctx, key);
}
std::vector<score::PluginKey> score_plugin_avnd::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_avnd)
