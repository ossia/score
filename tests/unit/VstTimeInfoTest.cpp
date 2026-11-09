// What a VST 2 plug-in reads with audioMasterGetTime before its process
// plays, while it is told it is about to (effMainsChanged), and once it has
// stopped: a stopped transport with a tempo, a rate and a signature, never
// whatever the memory of the wrapper held.

#include <Vst/Node.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <new>

namespace
{
struct fake_plugin
{
  AEffect effect{};
  vst::AEffectWrapper* wrapper{};
  VstTimeInfo at_resume{};
  VstTimeInfo while_playing{};

  static fake_plugin& self(AEffect* e) { return *static_cast<fake_plugin*>(e->user); }

  static intptr_t dispatcher(AEffect* e, int32_t op, int32_t, intptr_t v, void*, float)
  {
    if(op == effMainsChanged && v == 1)
      self(e).at_resume = self(e).wrapper->info;
    return 0;
  }
  static void process(AEffect* e, float**, float**, int32_t)
  {
    self(e).while_playing = self(e).wrapper->info;
  }
  static void setParameter(AEffect*, int32_t, float) { }
  static float getParameter(AEffect*, int32_t) { return 0.f; }

  fake_plugin()
  {
    effect.magic = kEffectMagic;
    effect.dispatcher = dispatcher;
    effect.processReplacing = process;
    effect.setParameter = setParameter;
    effect.getParameter = getParameter;
    effect.numInputs = 2;
    effect.numOutputs = 2;
    effect.user = this;
  }
};

bool sane(const VstTimeInfo& t)
{
  return std::isfinite(t.sampleRate) && t.sampleRate > 0 && std::isfinite(t.tempo)
         && t.tempo > 0 && t.timeSigNumerator > 0 && t.timeSigDenominator > 0
         && std::isfinite(t.samplePos) && std::isfinite(t.ppqPos);
}
}

TEST_CASE("VST 2: the time info is a stopped transport until the process plays", "[vst]")
{
  fake_plugin plugin;
  // Memory a previous allocation left full of ones
  alignas(vst::AEffectWrapper) unsigned char storage[sizeof(vst::AEffectWrapper)];
  std::memset(storage, 0xff, sizeof(storage));
  auto* wrapper = new(storage) vst::AEffectWrapper{&plugin.effect};
  plugin.wrapper = wrapper;

  CHECK(sane(wrapper->info));
  CHECK(!(wrapper->info.flags & kVstTransportPlaying));

  // Not owned: the node holds it without deleting it
  std::shared_ptr<vst::AEffectWrapper> shared{wrapper, [](vst::AEffectWrapper*) { }};
  ossia::execution_state state;
  state.sampleRate = 48000;
  state.bufferSize = 512;
  {
    vst::vst_node<false, false> node{shared, 48000, 512};
    CHECK(sane(plugin.at_resume));
    CHECK(!(plugin.at_resume.flags & kVstTransportPlaying));

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{512};
    tk.start_sample = 0;
    tk.length_sample = 512;
    tk.tempo = 90.;
    node.run(tk, ossia::exec_state_facade{&state});
    CHECK(plugin.while_playing.flags & kVstTransportPlaying);
    CHECK(plugin.while_playing.tempo == 90.);
  }
  // Stopped
  CHECK(!(wrapper->info.flags & kVstTransportPlaying));
  CHECK(wrapper->info.flags & kVstTransportChanged);

  // The wrapper would ask the application to close the plug-in
  wrapper->fx = nullptr;
  wrapper->~AEffectWrapper();
}
