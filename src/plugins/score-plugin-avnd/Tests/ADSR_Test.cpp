// ao::ADSR: the Hold gate and the Trigger impulse, at the object level and
// through the real ossia binding.
//
// The object has two envelopes sharing one control output: `hold` gates a
// gam::ADSR, `trig` fires a gam::AD. The output is one value per buffer, so
// every assertion here is in buffers, not samples.
//
// The binding-level cases cover the cable path (a value_port entry with a real
// timestamp) and the OSC path (add_global_value, always timestamp 0). The UI
// path does not touch the port at all and is covered in
// tests/unit/AvndAdsrExecTest.cpp.

#include <ossia/dataflow/execution_state.hpp>

#include <Advanced/Utilities/ADSR.hpp>
#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <avnd/wrappers/controls.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using Catch::Approx;

namespace
{
//! avnd::init_controls wants something with .inputs and .effect; the raw object
//! is not in an avnd::effect_container here.
struct control_init_state
{
  ao::ADSR& effect;
  decltype(ao::ADSR::inputs)& inputs;
};

struct fixture
{
  ao::ADSR fx;

  explicit fixture(double rate = 48000., int frames = 64)
  {
    control_init_state st{fx, fx.inputs};
    avnd::init_controls(st);
    prepare(rate, frames);
  }

  void prepare(double rate, int frames)
  {
    fx.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = frames, .rate = rate});
  }

  void times(float attack, float decay, float release)
  {
    fx.inputs.attack.value = attack;
    fx.inputs.attack.update(fx);
    fx.inputs.decay.value = decay;
    fx.inputs.decay.update(fx);
    fx.inputs.release.value = release;
    fx.inputs.release.update(fx);
  }

  //! One buffer. The host clears the impulse after every tick: do the same.
  float tick(int frames = 64)
  {
    fx(frames);
    fx.inputs.trig.value.reset();
    return fx.outputs.out.value;
  }

  void bang() { fx.inputs.trig.value.emplace(); }

  //! Run until the output comes back to zero, gathering the envelope.
  std::vector<float> until_silent(int frames = 64, int max_blocks = 20000)
  {
    std::vector<float> out;
    for(int i = 0; i < max_blocks; i++)
    {
      out.push_back(tick(frames));
      if(i > 0 && out.back() == 0.f)
        break;
    }
    return out;
  }
};

float peak_of(const std::vector<float>& v)
{
  float m = 0.f;
  for(float x : v)
    m = x > m ? x : m;
  return m;
}
}

TEST_CASE("ADSR: a trigger plays one AD envelope and returns to zero", "[avnd][adsr]")
{
  fixture f;
  f.times(0.01f, 0.01f, 0.01f);

  CHECK(f.tick() == 0.f);

  f.bang();
  auto env = f.until_silent();

  // 0.02 s of envelope at 48 kHz in blocks of 64 = 15 blocks
  CHECK(env.size() >= 14);
  CHECK(env.size() <= 18);
  CHECK(peak_of(env) == Approx(1.f).margin(0.01));
  CHECK(env.back() == 0.f);

  // ... and it stays at zero: the output is not left stale from the last
  // buffer that had any envelope in it.
  for(int i = 0; i < 10; i++)
    CHECK(f.tick() == 0.f);
}

TEST_CASE("ADSR: Hold rises to sustain, holds, then releases", "[avnd][adsr]")
{
  fixture f;
  f.times(0.01f, 0.01f, 0.01f);
  f.fx.inputs.sustain.value = 0.25f;
  f.fx.inputs.sustain.update(f.fx);

  f.fx.inputs.hold = true;
  float last = 0.f;
  for(int i = 0; i < 30; i++)
    last = f.tick();
  CHECK(last == Approx(0.25f).margin(0.001));

  // Held: the value does not move as long as the gate is down
  for(int i = 0; i < 50; i++)
    CHECK(f.tick() == Approx(0.25f).margin(0.001));

  f.fx.inputs.hold = false;
  auto rel = f.until_silent();
  CHECK(rel.size() >= 6);
  CHECK(rel.size() <= 14);
  CHECK(rel.front() < 0.26f);
  CHECK(rel.back() == 0.f);
}

TEST_CASE("ADSR: a trigger during Hold never steps the output", "[avnd][adsr]")
{
  fixture f;
  f.times(0.05f, 0.05f, 0.05f);
  f.fx.inputs.sustain.value = 0.5f;
  f.fx.inputs.sustain.update(f.fx);

  f.fx.inputs.hold = true;
  f.bang();

  // The AD peaks at 1 and decays back below the sustain level. Once past that
  // peak the output only ever goes down, down to the sustain level: the end of
  // the AD must not step the output back up to it.
  float prev = 0.f;
  float top = 0.f;
  float worst_rise = 0.f;
  for(int i = 0; i < 400; i++)
  {
    const float v = f.tick(256);
    if(v >= top)
      top = v;
    else if(v - prev > worst_rise)
      worst_rise = v - prev;
    prev = v;
  }

  CHECK(top == Approx(1.f).margin(0.01));
  CHECK(prev == Approx(0.5f).margin(0.001));
  CHECK(worst_rise == 0.f);
}

TEST_CASE("ADSR: an envelope shorter than one buffer is still reported", "[avnd][adsr]")
{
  fixture f{48000., 512};
  // The bottom of the knob range: 0.1 ms, a fifth of a 512-sample buffer
  f.times(0.0001f, 0.0001f, 0.0001f);

  CHECK(f.tick(512) == 0.f);
  f.bang();
  CHECK(f.tick(512) == Approx(1.f).margin(0.01));
  CHECK(f.tick(512) == 0.f);
}

TEST_CASE("ADSR: envelope times are in seconds at any sample rate", "[avnd][adsr]")
{
  const auto duration_of = [](double rate) {
    fixture f{rate, 16};
    f.times(0.01f, 0.01f, 0.01f);
    f.bang();
    auto env = f.until_silent(16);
    CHECK(peak_of(env) == Approx(1.f).margin(0.01));
    return env.size() * 16. / rate;
  };

  CHECK(duration_of(48000.) == Approx(0.02).margin(0.002));
  CHECK(duration_of(96000.) == Approx(0.02).margin(0.002));
}

TEST_CASE("ADSR: Hold already down when prepare() runs is not an edge", "[avnd][adsr]")
{
  fixture f;
  f.times(0.01f, 0.01f, 0.01f);

  // The executor pushes the model's control values before it calls prepare()
  f.fx.inputs.hold = true;
  f.prepare(48000., 64);

  for(int i = 0; i < 20; i++)
    CHECK(f.tick() == 0.f);

  // ... and the gate still works from there: lifting it, then pressing it
  f.fx.inputs.hold = false;
  CHECK(f.tick() == 0.f);
  f.fx.inputs.hold = true;
  float last = 0.f;
  for(int i = 0; i < 30; i++)
    last = f.tick();
  CHECK(last == Approx(0.5f).margin(0.001));
}

TEST_CASE("ADSR: a trigger restarts the AD from zero", "[avnd][adsr]")
{
  fixture f;
  f.times(0.05f, 0.05f, 0.05f);

  f.bang();
  for(int i = 0; i < 10; i++)
    f.tick();
  const float before = f.fx.outputs.out.value;
  CHECK(before > 0.1f);

  f.bang();
  CHECK(f.tick() < before);
}

// ---------------------------------------------------------------------------
// Binding level: real ossia ports, real ticks.
// ---------------------------------------------------------------------------

namespace
{
struct host_fixture
{
  ossia::execution_state state;
  oscr::safe_node<ao::ADSR> host;

  static constexpr int hold_port = 0;
  static constexpr int trig_port = 1;

  explicit host_fixture(int bufsize = 256, double rate = 48000.)
      : host{bufsize, rate, 0}
  {
    state.bufferSize = bufsize;
    state.sampleRate = rate;
    state.modelToSamplesRatio = 1.;
    state.samplesToModelRatio = 1.;
    host.finish_init();
    // What Crousti's Executor does once everything is connected: this is the
    // only thing that ever calls the object's prepare().
    host.audio_configuration_changed(ossia::exec_state_facade{&state});
  }

  ao::ADSR& object() { return host.impl.effect; }

  ossia::value_inlet& inlet(int n)
  {
    return n == hold_port ? tuplet::get<hold_port>(host.ossia_inlets.ports)
                          : tuplet::get<trig_port>(host.ossia_inlets.ports);
  }

  ossia::value_outlet& outlet() { return tuplet::get<0>(host.ossia_outlets.ports); }

  void times(float attack, float decay, float release)
  {
    auto& o = object();
    o.inputs.attack.value = attack;
    o.inputs.attack.update(o);
    o.inputs.decay.value = decay;
    o.inputs.decay.update(o);
    o.inputs.release.value = release;
    o.inputs.release.update(o);
  }

  //! One tick of `frames` samples, then the clean-up the graph does.
  float run(int frames = 256)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{frames};
    tk.start_sample = 0;
    tk.length_sample = frames;
    host.run(tk, ossia::exec_state_facade{&state});

    float out = 0.f;
    auto& data = outlet()->get_data();
    if(!data.empty())
      out = ossia::convert<float>(data.back().value);
    outlet()->clear();
    inlet(hold_port).data.clear();
    inlet(trig_port).data.clear();
    return out;
  }
};
}

TEST_CASE("ADSR: the executor's setup gives the object its sample rate", "[avnd][adsr]")
{
  // finish_init() alone does not: audio_configuration_changed() is what calls
  // prepare(), and a data node never reaches it from prepare_run().
  host_fixture f;
  f.times(0.01f, 0.01f, 0.01f);
  f.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 0);

  int blocks = 0;
  float peak = 0.f;
  for(int i = 0; i < 200; i++)
  {
    const float v = f.run(64);
    peak = v > peak ? v : peak;
    blocks++;
    if(i > 0 && v == 0.f)
      break;
  }
  CHECK(peak == Approx(1.f).margin(0.01));
  // 0.02 s at 48 kHz in blocks of 64: if the rate had stayed at 0 the envelope
  // would be over in one block.
  CHECK(blocks >= 14);
}

TEST_CASE("ADSR: a cable impulse fires the envelope once", "[avnd][adsr]")
{
  host_fixture f;
  f.times(0.01f, 0.01f, 0.01f);

  CHECK(f.run(64) == 0.f);

  f.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 17);
  CHECK(f.run(64) > 0.f);
  // The binding clears the optional in finish_run(): the next tick is not a
  // second trigger.
  CHECK(!f.object().inputs.trig);

  float prev = f.object().outputs.out.value;
  bool rose = false;
  for(int i = 0; i < 40; i++)
  {
    const float v = f.run(64);
    if(v > prev && v > 0.f)
      rose = true;
    prev = v;
    if(v == 0.f)
      break;
  }
  CHECK(rose); // it kept climbing its attack instead of restarting
  CHECK(f.object().outputs.out.value == 0.f);
}

TEST_CASE("ADSR: an impulse inlet bangs on any value it receives", "[avnd][adsr]")
{
  // oscr::from_ossia_value engages an optional with an empty payload whatever
  // arrives: an event port reports that an event happened, not what it was.
  // This is what lets `/adsr/trigger 0` from OSC work, and it is also why a
  // continuously-emitting source cabled into Trigger retriggers every tick.
  for(const ossia::value& v :
      {ossia::value{ossia::impulse{}}, ossia::value{false}, ossia::value{0},
       ossia::value{0.f}, ossia::value{std::string{"go"}}})
  {
    host_fixture f;
    f.times(0.01f, 0.01f, 0.01f);
    f.inlet(host_fixture::trig_port).data.write_value(v, 0);
    f.run(64);
    CHECK(f.object().outputs.out.value > 0.f);
  }
}

TEST_CASE("ADSR: a source that emits every tick pins the envelope", "[avnd][adsr]")
{
  // So an automation cabled into Trigger restarts the AD on every buffer and
  // never leaves the foot of its attack: nothing in the object can tell such a
  // cable apart from a deliberate machine-gun trigger.
  host_fixture f;
  f.times(0.25f, 0.25f, 0.25f);

  float pinned = 0.f;
  for(int i = 0; i < 40; i++)
  {
    f.inlet(host_fixture::trig_port).data.write_value(0.5f, 0);
    pinned = f.run(64);
  }
  // One 64-sample buffer at the foot of a 0.25 s attack, over and over
  CHECK(pinned < 0.05f);

  // Left alone, the same envelope climbs away from there
  for(int i = 0; i < 40; i++)
    f.run(64);
  CHECK(f.object().outputs.out.value > 0.5f);
}

TEST_CASE("ADSR: several impulses in one tick are one trigger", "[avnd][adsr]")
{
  // process_before_run reads only the last value of the port and drops its
  // timestamp. The output is one value per buffer, so a second restart inside
  // the same buffer could not be observed at the outlet anyway.
  host_fixture f;
  f.times(0.05f, 0.05f, 0.05f);

  f.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 0);
  f.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 100);
  f.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 200);
  const float many = f.run(256);

  host_fixture g;
  g.times(0.05f, 0.05f, 0.05f);
  g.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 0);
  const float one = g.run(256);

  CHECK(many == Approx(one));
}

TEST_CASE("ADSR: Hold over a cable and over OSC behave the same", "[avnd][adsr]")
{
  // A cable carries a real timestamp; values received from the network are
  // pushed by add_global_value with timestamp 0. Neither reaches the object:
  // the control path keeps the last value of the tick and nothing else.
  const auto gate = [](int64_t press_ts) {
    host_fixture f;
    f.times(0.01f, 0.01f, 0.01f);
    f.inlet(host_fixture::hold_port).data.write_value(true, press_ts);
    float last = 0.f;
    for(int i = 0; i < 30; i++)
      last = f.run(64);
    CHECK(f.object().inputs.hold == true);
    CHECK(last == Approx(0.5f).margin(0.001));

    f.inlet(host_fixture::hold_port).data.write_value(false, press_ts);
    for(int i = 0; i < 40; i++)
    {
      last = f.run(64);
      if(last == 0.f)
        break;
    }
    CHECK(f.object().inputs.hold == false);
    return last;
  };

  CHECK(gate(0) == 0.f);  // OSC: add_global_value always stamps 0
  CHECK(gate(31) == 0.f); // cable: a real timestamp inside the buffer
}

TEST_CASE("ADSR: the outlet gets one value per tick", "[avnd][adsr]")
{
  host_fixture f;
  f.times(0.01f, 0.01f, 0.01f);
  f.inlet(host_fixture::trig_port).data.write_value(ossia::impulse{}, 0);

  ossia::token_request tk;
  tk.date = ossia::time_value{64};
  tk.start_sample = 0;
  tk.length_sample = 64;
  f.host.run(tk, ossia::exec_state_facade{&f.state});

  auto& data = f.outlet()->get_data();
  REQUIRE(data.size() == 1);
  CHECK(data[0].timestamp == 0);
  CHECK(data[0].value.get_type() == ossia::val_type::FLOAT);
}
