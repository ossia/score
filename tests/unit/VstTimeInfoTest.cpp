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

namespace
{
//! Records the transport position of each of its process calls
struct position_recorder
{
  AEffect effect{};
  vst::AEffectWrapper* wrapper{};
  std::vector<double> sample_pos;

  static position_recorder& self(AEffect* e)
  {
    return *static_cast<position_recorder*>(e->user);
  }
  static intptr_t dispatcher(AEffect*, int32_t, int32_t, intptr_t, void*, float) { return 0; }
  static void process(AEffect* e, float**, float**, int32_t)
  {
    self(e).sample_pos.push_back(self(e).wrapper->info.samplePos);
  }
  static void setParameter(AEffect*, int32_t, float) { }
  static float getParameter(AEffect*, int32_t) { return 0.f; }

  position_recorder()
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
}

TEST_CASE(
    "VST 2: a block is told the transport position of its own first sample", "[vst]")
{
  position_recorder plugin;
  auto* wrapper = new vst::AEffectWrapper{&plugin.effect};
  plugin.wrapper = wrapper;
  std::shared_ptr<vst::AEffectWrapper> shared{wrapper, [](vst::AEffectWrapper*) { }};

  ossia::execution_state state;
  state.sampleRate = 48000;
  state.bufferSize = 512;
  state.modelToSamplesRatio = 1.;
  state.samplesToModelRatio = 1.;

  struct tick_t
  {
    int64_t prev, date, start_sample, length;
    double speed;
  };
  // Dates in samples: a play from 0, a tick starting inside its buffer, a
  // loop back to 0, a seek, twice the speed, then backwards
  const tick_t ticks[]{
      {0, 512, 0, 512, 1.},         {512, 1024, 0, 512, 1.},
      {1024, 1388, 148, 364, 1.},   {1388, 1900, 0, 512, 1.},
      {0, 512, 0, 512, 1.},         {48000, 48512, 0, 512, 1.},
      {48512, 49536, 0, 512, 2.},   {49536, 49024, 0, 512, -1.},
      {49024, 48512, 0, 512, -1.}};

  std::vector<double> expected;
  {
    vst::vst_node<false, false> node{shared, 48000, 512};
    for(const auto& t : ticks)
    {
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{t.prev};
      tk.date = ossia::time_value{t.date};
      tk.start_sample = t.start_sample;
      tk.length_sample = t.length;
      tk.speed = t.speed;
      // As the graph runs a node: where the tick starts first, the time it
      // covered after
      node.begin_request(tk, state);
      node.run(tk, ossia::exec_state_facade{&state});
      node.process_time(tk, state);
      expected.push_back(double(tk.start_date_to_physical(state.modelToSamplesRatio)));
    }
  }
  CHECK(plugin.sample_pos == expected);

  wrapper->fx = nullptr;
  delete wrapper;
}

TEST_CASE(
    "VST 2: the blocks of a tick played backwards go back in time", "[vst]")
{
  position_recorder plugin;
  auto* wrapper = new vst::AEffectWrapper{&plugin.effect};
  plugin.wrapper = wrapper;
  std::shared_ptr<vst::AEffectWrapper> shared{wrapper, [](vst::AEffectWrapper*) { }};

  ossia::execution_state state;
  state.sampleRate = 48000;
  state.bufferSize = 1024;
  state.modelToSamplesRatio = 1.;
  state.samplesToModelRatio = 1.;
  {
    // Blocks of 256: a tick of 1024 reaches the plug-in in four
    vst::vst_node<false, false> node{shared, 48000, 256};
    for(double speed : {1., -1.})
    {
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{4096};
      tk.date = ossia::time_value{speed > 0 ? 5120 : 3072};
      tk.start_sample = 0;
      tk.length_sample = 1024;
      tk.speed = speed;
      node.begin_request(tk, state);
      node.run(tk, ossia::exec_state_facade{&state});
      node.process_time(tk, state);
    }
  }
  CHECK(
      plugin.sample_pos
      == std::vector<double>{4096, 4352, 4608, 4864, 4096, 3840, 3584, 3328});

  wrapper->fx = nullptr;
  delete wrapper;
}
