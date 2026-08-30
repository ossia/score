// JS::js_node -- the CPU executor of the Javascript process -- driven the way
// the audio graph drives it: ports wired by hand, a script installed with
// setScript(), then run() called once per tick with a real token and execution
// state.
//
// JsPresetsTest.cpp ticks the QML object directly and never touches the node,
// so nothing so far covers the half of CPUNode.cpp that copies samples,
// messages and MIDI across the QML boundary. This does, and it asserts the
// samples by value rather than asserting that a tick happened.
//
// The node asserts it is running on the audio thread; the test thread is the UI
// thread, and ossia::ensure_current_thread only prints when that is not so, so
// the runs are correct but noisy. Nothing here depends on that message.

#include <score_test/App.hpp>

#include <JS/Executor/CPUNode.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;

namespace
{
constexpr int64_t flicks_per_sample(int rate)
{
  return 705600000 / rate;
}

ossia::token_request make_token(int tick, int rate, int buffer)
{
  const int64_t per_buffer = flicks_per_sample(rate) * buffer;
  ossia::token_request tk;
  tk.prev_date = ossia::time_value{per_buffer * tick};
  tk.date = ossia::time_value{per_buffer * (tick + 1)};
  tk.parent_duration = ossia::time_value{per_buffer * 256};
  tk.speed = 1.;
  tk.tempo = 120.;
  tk.signature = ossia::time_signature{4, 4};
  tk.start_sample = 0;
  tk.length_sample = buffer;
  return tk;
}

//! One js_node with its ports, its script and its execution state. Ports must
//! be added before setScript(): setupComponent() pairs the n-th QML port
//! declaration with the n-th ossia port by index.
struct rig
{
  ossia::execution_state st;
  std::shared_ptr<JS::js_node> node;
  int tick_index{};

  explicit rig(int rate = 48000, int buffer = 64)
  {
    st.sampleRate = rate;
    st.bufferSize = buffer;
    node = std::make_shared<JS::js_node>(st);
  }

  ~rig()
  {
    if(node)
      node->clear();
  }

  void audio_in() { node->root_inputs().push_back(new ossia::audio_inlet); }
  void value_in() { node->root_inputs().push_back(new ossia::value_inlet); }
  void midi_in() { node->root_inputs().push_back(new ossia::midi_inlet); }
  void audio_out() { node->root_outputs().push_back(new ossia::audio_outlet); }
  void value_out() { node->root_outputs().push_back(new ossia::value_outlet); }
  void midi_out() { node->root_outputs().push_back(new ossia::midi_outlet); }

  void script(const char* src) { node->setScript({}, QString::fromUtf8(src)); }

  void run()
  {
    node->run(
        make_token(tick_index++, st.sampleRate, st.bufferSize),
        ossia::exec_state_facade{&st});
  }

  ossia::audio_vector& out_audio(int i = 0)
  {
    return node->root_outputs()[i]->template target<ossia::audio_port>()->get();
  }
  ossia::audio_vector& in_audio(int i = 0)
  {
    return node->root_inputs()[i]->template target<ossia::audio_port>()->get();
  }
  ossia::value_port& out_values(int i = 0)
  {
    return *node->root_outputs()[i]->template target<ossia::value_port>();
  }
  ossia::value_port& in_values(int i = 0)
  {
    return *node->root_inputs()[i]->template target<ossia::value_port>();
  }
  ossia::midi_port& out_midi(int i = 0)
  {
    return *node->root_outputs()[i]->template target<ossia::midi_port>();
  }
  ossia::midi_port& in_midi(int i = 0)
  {
    return *node->root_inputs()[i]->template target<ossia::midi_port>();
  }

  //! What the graph does between ticks: the ports are the graph's, not the
  //! node's, and they are emptied before every tick.
  void clear_ports()
  {
    for(auto& o : node->root_outputs())
    {
      if(auto p = o->template target<ossia::audio_port>())
        p->get().clear();
      else if(auto v = o->template target<ossia::value_port>())
        v->get_data().clear();
      else if(auto m = o->template target<ossia::midi_port>())
        m->messages.clear();
    }
  }
};

//! An ossia::audio_port is born with two empty channels, so "the node wrote
//! nothing" is "no channel holds a sample", not "the port is empty".
bool no_samples(const ossia::audio_vector& v)
{
  for(const auto& ch : v)
    if(!ch.empty())
      return false;
  return true;
}

const char* ramp_script = R"_(import Score
Script {
  AudioOutlet { id: out; objectName: "out" }
  tick: function(token, state) {
    var n = state.timings(token).length;
    var buf = new Array(n);
    for(var i = 0; i < n; i++)
      buf[i] = i / 100.0;
    out.setChannel(0, buf);
  }
})_";
}

TEST_CASE("A Javascript node fills its audio outlet with the samples it wrote",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.audio_out();
    r.script(ramp_script);
    r.run();

    auto& out = r.out_audio();
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].size() == 64);
    CHECK(out[0][0] == Approx(0.0));
    CHECK(out[0][1] == Approx(0.01));
    CHECK(out[0][10] == Approx(0.10));
    CHECK(out[0][63] == Approx(0.63));
  });
}

TEST_CASE("A Javascript node reads the samples in its audio inlet",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.audio_in();
    r.audio_out();
    r.script(R"_(import Score
Script {
  AudioInlet { id: ain; objectName: "in" }
  AudioOutlet { id: aout; objectName: "out" }
  tick: function(token, state) {
    var c = ain.channel(0);
    var o = new Array(c.length);
    for(var i = 0; i < c.length; i++)
      o[i] = 2.0 * c[i];
    aout.setChannel(0, o);
  }
})_");

    auto& in = r.in_audio();
    in.resize(1);
    in[0].resize(64);
    for(int i = 0; i < 64; i++)
      in[0][i] = 0.5 * i;

    r.run();

    auto& out = r.out_audio();
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].size() == 64);
    CHECK(out[0][0] == Approx(0.0));
    CHECK(out[0][1] == Approx(1.0));
    CHECK(out[0][63] == Approx(63.0));
  });
}

TEST_CASE("A Javascript node follows a change of buffer size and channel count",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.audio_in();
    r.audio_out();
    r.script(R"_(import Score
Script {
  AudioInlet { id: ain; objectName: "in" }
  AudioOutlet { id: aout; objectName: "out" }
  tick: function(token, state) {
    var n = state.timings(token).length;
    for(var ch = 0; ch < 2; ch++) {
      var c = ain.channel(ch);
      var o = new Array(n);
      for(var i = 0; i < n; i++)
        o[i] = (i < c.length) ? c[i] : -1.0;
      aout.setChannel(ch, o);
    }
  }
})_");

    SECTION("one channel, 64 samples, then two channels, 128 samples")
    {
      auto& in = r.in_audio();
      in.resize(1);
      in[0].assign(64, 0.25);
      r.run();
      {
        auto& out = r.out_audio();
        REQUIRE(out.size() == 2);
        REQUIRE(out[0].size() == 64);
        CHECK(out[0][0] == Approx(0.25));
        CHECK(out[0][63] == Approx(0.25));
        // The second channel has no input at all: the script's own fallback.
        REQUIRE(out[1].size() == 64);
        CHECK(out[1][0] == Approx(-1.0));
      }

      r.clear_ports();
      r.st.bufferSize = 128;
      auto& in2 = r.in_audio();
      in2.resize(2);
      in2[0].assign(128, 0.5);
      in2[1].assign(128, 0.75);
      r.node->root_inputs();
      r.tick_index = 0;
      r.node->run(
          make_token(1, r.st.sampleRate, 128), ossia::exec_state_facade{&r.st});

      auto& out = r.out_audio();
      REQUIRE(out.size() == 2);
      REQUIRE(out[0].size() == 128);
      REQUIRE(out[1].size() == 128);
      CHECK(out[0][127] == Approx(0.5));
      CHECK(out[1][127] == Approx(0.75));
    }
  });
}

// js_node::run clears the QML value outlets (js_port.clear()) and the QML MIDI
// outlets (->clear()) after writing them out, but nothing clears the QML audio
// outlet: JS::AudioOutlet has no clear() at all. The graph empties the ossia
// port between ticks, so on the next tick the node copies the SAME samples out
// again -- a script that writes on one tick and stays silent afterwards keeps
// emitting that one buffer for as long as it runs.
//
// Measured: tick 1 writes 64 samples of 0.5, tick 2 writes nothing, and the
// outlet still carries 64 samples of 0.5.
TEST_CASE("A Javascript node that writes no audio emits no audio",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.audio_out();
    r.script(R"_(import Score
Script {
  AudioOutlet { id: out; objectName: "out" }
  property int n: 0
  tick: function(token, state) {
    n = n + 1;
    if(n === 1) {
      var buf = new Array(64);
      for(var i = 0; i < 64; i++)
        buf[i] = 0.5;
      out.setChannel(0, buf);
    }
  }
})_");

    r.run();
    REQUIRE(r.out_audio()[0].size() == 64);
    CHECK(r.out_audio()[0][0] == Approx(0.5));

    r.clear_ports();
    r.run();
    CHECK(no_samples(r.out_audio()));
  });
}

TEST_CASE("A Javascript node passes value messages through with their timestamps",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.value_in();
    r.value_out();
    r.script(R"_(import Score
Script {
  ValueInlet { id: vin; objectName: "in" }
  ValueOutlet { id: vout; objectName: "out" }
  tick: function(token, state) {
    var vs = vin.values;
    for(var i = 0; i < vs.length; i++)
      vout.addValue(vs[i].timestamp, vs[i].value * 10);
  }
})_");

    auto& in = r.in_values();
    in.write_value(ossia::value{1.f}, 0);
    in.write_value(ossia::value{2.f}, 16);
    in.write_value(ossia::value{3.f}, 32);

    r.run();

    auto& out = r.out_values().get_data();
    REQUIRE(out.size() == 3);
    CHECK(ossia::convert<float>(out[0].value) == Approx(10.f));
    CHECK(ossia::convert<float>(out[1].value) == Approx(20.f));
    CHECK(ossia::convert<float>(out[2].value) == Approx(30.f));
    CHECK(out[0].timestamp == 0);
    CHECK(out[1].timestamp == 16);
    CHECK(out[2].timestamp == 32);
  });
}

TEST_CASE("A Javascript node receives control values and impulses",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    // Both a ControlInlet and an Impulse are carried by an ossia::value_inlet:
    // js_node::run reads the last value of the first and counts the messages
    // of the second.
    r.value_in();
    r.value_in();
    r.value_out();
    r.script(R"_(import Score
Script {
  id: root
  property int bangs: 0
  FloatSlider { id: sl; objectName: "sl"; min: 0; max: 10; init: 1 }
  Impulse { id: imp; objectName: "imp"; onImpulse: root.bangs = root.bangs + 1 }
  ValueOutlet { id: out; objectName: "out" }
  tick: function(token, state) { out.value = [sl.value, root.bangs]; }
})_");

    // Only the last control value of a tick reaches the script.
    r.in_values(0).write_value(ossia::value{2.f}, 0);
    r.in_values(0).write_value(ossia::value{7.5f}, 32);
    // Two bangs in one tick must be two calls, not one.
    r.in_values(1).write_value(ossia::impulse{}, 0);
    r.in_values(1).write_value(ossia::impulse{}, 16);

    r.run();

    auto& out = r.out_values().get_data();
    REQUIRE(out.size() == 1);
    auto* lst = out[0].value.target<std::vector<ossia::value>>();
    REQUIRE(lst != nullptr);
    REQUIRE(lst->size() == 2);
    CHECK(ossia::convert<float>((*lst)[0]) == Approx(7.5f));
    CHECK(ossia::convert<int>((*lst)[1]) == 2);
  });
}

TEST_CASE("A Javascript node emits the MIDI its script builds",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.midi_out();
    r.script(R"_(import Score
Script {
  MidiOutlet { id: mout; objectName: "out" }
  tick: function(token, state) {
    mout.add([0x90, 64, 77]);
  }
})_");

    r.run();

    auto& out = r.out_midi().messages;
    REQUIRE(out.size() == 1);
    const auto m1 = libremidi::midi1_from_ump(out[0]);
    REQUIRE(m1.bytes.size() == 3);
    CHECK((int)m1.bytes[0] == 0x90);
    CHECK((int)m1.bytes[1] == 64);
    CHECK((int)m1.bytes[2] == 77);
  });
}

// JS::MidiInlet::setMidi (QmlObjects.hpp) sizes the array it hands the script
// with mess.size() -- the number of 32-BIT WORDS in the UMP -- and then fills
// it from m1.bytes, which is the MIDI 1.0 BYTE array. ump_from_midi1 produces
// a MIDI-2 channel-voice UMP, whose size() is 2, so a three-byte note-on
// reaches the script as [status, note]: the velocity is gone, and so is the
// value of every control change, the pressure of every aftertouch and the
// second byte of every pitch bend.
//
// Measured: the script sees length 2 and [144, 60] for a note-on 60/100.
//
// The two counts are unrelated in the other direction too: a UMP whose MIDI 1
// conversion fails yields an empty m1.bytes while size() still says 1 or 2, so
// the loop reads past the end of the message.
TEST_CASE("A Javascript node receives whole MIDI messages",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.midi_in();
    r.value_out();
    r.script(R"_(import Score
Script {
  MidiInlet { id: min; objectName: "in" }
  ValueOutlet { id: vout; objectName: "out" }
  tick: function(token, state) {
    var ms = min.messages();
    if(ms.length > 0)
      vout.value = ms[0].length;
  }
})_");

    r.in_midi().messages.push_back(
        libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 60, 100)));

    r.run();

    auto& out = r.out_values().get_data();
    REQUIRE(out.size() == 1);
    // A note-on is three bytes. The script is handed two.
    CHECK(ossia::convert<int>(out[0].value) == 3);
  });
}

TEST_CASE("A Javascript node whose tick throws keeps running",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.value_out();
    r.script(R"_(import Score
Script {
  ValueOutlet { id: vout; objectName: "out" }
  property int n: 0
  tick: function(token, state) {
    n = n + 1;
    if(n === 1)
      throw new Error("boom");
    vout.value = n;
  }
})_");

    // Tick 1 throws: nothing is emitted, and the process is still here.
    r.run();
    CHECK(r.out_values().get_data().empty());

    // Tick 2 runs normally.
    r.clear_ports();
    r.run();
    auto& out = r.out_values().get_data();
    REQUIRE(out.size() == 1);
    CHECK(ossia::convert<int>(out[0].value) == 2);
  });
}

TEST_CASE("A Javascript node given a script that does not parse degrades to a no-op",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    SECTION("syntax error")
    {
      rig r{48000, 64};
      r.audio_out();
      r.script("import Score\nScript { this is not qml ((( }");
      r.run();
      CHECK(no_samples(r.out_audio()));
    }

    SECTION("parses, but the root object is not a Script")
    {
      rig r{48000, 64};
      r.audio_out();
      r.script("import QtQml\nQtObject { }");
      r.run();
      CHECK(no_samples(r.out_audio()));
    }

    SECTION("a Script with no tick function")
    {
      rig r{48000, 64};
      r.audio_out();
      r.script("import Score\nScript { AudioOutlet { objectName: \"out\" } }");
      r.run();
      CHECK(no_samples(r.out_audio()));
    }
  });
}

TEST_CASE("A Javascript node replaced mid-flight runs the new script",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    rig r{48000, 64};
    r.value_out();
    r.script(R"_(import Score
Script {
  ValueOutlet { id: vout; objectName: "out" }
  tick: function(token, state) { vout.value = 11; }
})_");
    r.run();
    {
      auto& out = r.out_values().get_data();
      REQUIRE(out.size() == 1);
      CHECK(ossia::convert<int>(out[0].value) == 11);
    }

    r.clear_ports();
    r.script(R"_(import Score
Script {
  ValueOutlet { id: vout; objectName: "out" }
  tick: function(token, state) { vout.value = 22; }
})_");
    r.run();
    {
      auto& out = r.out_values().get_data();
      REQUIRE(out.size() == 1);
      CHECK(ossia::convert<int>(out[0].value) == 22);
    }
  });
}

TEST_CASE("The same Javascript script and tokens produce the same samples twice",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    const auto play = [] {
      rig r{48000, 64};
      r.audio_out();
      r.script(R"_(import Score
Script {
  AudioOutlet { id: out; objectName: "out" }
  tick: function(token, state) {
    var n = state.timings(token).length;
    var t = token.previous_date;
    var buf = new Array(n);
    for(var i = 0; i < n; i++)
      buf[i] = Math.sin(0.01 * (t + i));
    out.setChannel(0, buf);
  }
})_");
      std::vector<double> all;
      for(int k = 0; k < 8; k++)
      {
        r.clear_ports();
        r.run();
        auto& o = r.out_audio();
        if(!o.empty())
          all.insert(all.end(), o[0].begin(), o[0].end());
      }
      return all;
    };

    const auto a = play();
    const auto b = play();
    REQUIRE(a.size() == 8 * 64);
    CHECK(a == b);
  });
}
