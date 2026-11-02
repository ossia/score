// A VST 2 plug-in is told the most frames it gets per call (effSetBlockSize).
// A tick longer than that, which another client of the audio server can
// impose, reaches it in several calls of at most that many frames, each with
// the MIDI events dated in it, and the transport where it starts.

#include <Vst/Node.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>

#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
struct call
{
  int32_t frames{};
  double sample_pos{};
  std::vector<std::pair<int, int32_t>> notes; // pitch, deltaFrames
};

struct fake_plugin
{
  AEffect effect{};
  std::vector<call> calls;
  std::vector<std::pair<int, int32_t>> pending;
  vst::AEffectWrapper* wrapper{};

  static fake_plugin& self(AEffect* e) { return *static_cast<fake_plugin*>(e->user); }

  static intptr_t dispatcher(AEffect* e, int32_t op, int32_t, intptr_t, void* ptr, float)
  {
    if(op == effProcessEvents)
    {
      auto* ev = static_cast<VstEvents*>(ptr);
      for(int i = 0; i < ev->numEvents; i++)
      {
        auto* m = reinterpret_cast<VstMidiEvent*>(ev->events[i]);
        self(e).pending.push_back({int(uint8_t(m->midiData[1])), m->deltaFrames});
      }
    }
    return 0;
  }
  static void process(AEffect* e, float**, float**, int32_t frames)
  {
    auto& s = self(e);
    s.calls.push_back({frames, s.wrapper->info.samplePos, std::move(s.pending)});
    s.pending.clear();
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
    effect.flags = effFlagsIsSynth;
    effect.user = this;
  }
};

libremidi::ump note(int pitch, int64_t frame)
{
  auto u = libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, pitch, 100));
  u.timestamp = frame;
  return u;
}
}

TEST_CASE(
    "VST 2: a tick longer than the block size is split, MIDI dated per block",
    "[vst][buffer_size][midi]")
{
  fake_plugin plugin;
  auto wrapper = std::make_shared<vst::AEffectWrapper>(&plugin.effect);
  plugin.wrapper = wrapper.get();

  constexpr int block = 256;
  ossia::execution_state state;
  state.sampleRate = 48000;
  state.bufferSize = 1000;
  {
    vst::vst_node<false, true> node{wrapper, 48000, block};
    auto& midi = static_cast<ossia::midi_inlet*>(node.root_inputs()[1])->data.messages;
    // In the first block, on the edge of the second, in the last, and
    // dated before and after the tick
    for(auto [pitch, frame] :
        {std::pair{60, 10}, {61, 256}, {62, 300}, {63, 999}, {64, -5}, {65, 4000}})
      midi.push_back(note(pitch, frame));

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{1000};
    tk.start_sample = 0;
    tk.length_sample = 1000;
    tk.tempo = 120.;
    node.run(tk, ossia::exec_state_facade{&state});
  }

  // 1000 frames: 256 + 256 + 256 + 232
  REQUIRE(plugin.calls.size() == 4);
  CHECK(plugin.calls[0].frames == block);
  CHECK(plugin.calls[1].frames == block);
  CHECK(plugin.calls[2].frames == block);
  CHECK(plugin.calls[3].frames == 1000 - 3 * block);

  for(std::size_t i = 0; i < plugin.calls.size(); i++)
    CHECK(plugin.calls[i].sample_pos == double(i * block));

  using notes = std::vector<std::pair<int, int32_t>>;
  // Before the tick: at its start
  CHECK(plugin.calls[0].notes == (notes{{60, 10}, {64, 0}}));
  // Relative to its block
  CHECK(plugin.calls[1].notes == (notes{{61, 0}, {62, 44}}));
  CHECK(plugin.calls[2].notes.empty());
  // After the tick: on its last frame
  CHECK(plugin.calls[3].notes == (notes{{63, 999 - 3 * block}, {65, 1000 - 3 * block - 1}}));

  // The wrapper would ask the application to close the plug-in
  wrapper->fx = nullptr;
}

TEST_CASE("VST 2: a tick within the block size is one call", "[vst][buffer_size]")
{
  fake_plugin plugin;
  auto wrapper = std::make_shared<vst::AEffectWrapper>(&plugin.effect);
  plugin.wrapper = wrapper.get();

  ossia::execution_state state;
  state.sampleRate = 48000;
  state.bufferSize = 512;
  {
    vst::vst_node<false, true> node{wrapper, 48000, 512};
    auto& midi = static_cast<ossia::midi_inlet*>(node.root_inputs()[1])->data.messages;
    midi.push_back(note(60, 100));

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{512};
    tk.start_sample = 0;
    tk.length_sample = 512;
    node.run(tk, ossia::exec_state_facade{&state});
  }
  REQUIRE(plugin.calls.size() == 1);
  CHECK(plugin.calls[0].frames == 512);
  CHECK(plugin.calls[0].notes == (std::vector<std::pair<int, int32_t>>{{60, 100}}));
  wrapper->fx = nullptr;
}
