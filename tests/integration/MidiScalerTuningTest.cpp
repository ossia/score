// The Midi scaler in a document, as the "scales" example uses it: a .scl file
// of the project on its "SCL file" port, notes on its MIDI inlet, its frequency
// output cabled to a Wavecycle. The notes must come out at the pitches of the
// scale at each step of the chain: the frequencies the scaler outputs, the
// pitch the Wavecycle renders, and the MIDI Tuning Standard data of the MTS
// output mode. A file chosen while playing applies too.
//
//   ctest -R integration_midi_scaler_tuning --output-on-failure
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QTemporaryDir>

#include <libremidi/detail/conversion.hpp>
#include <libremidi/message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace
{
const QString scaler_uuid = QStringLiteral("c43300b0-6c98-4a38-81f2-b549f0297e38");
const QString wavecycle_uuid = QStringLiteral("494bd8a3-e973-4fb0-b84b-b4ed3c0068a1");

constexpr double middle_c = 261.6255653;

// 19 equal steps to the octave: a step is 63.16 cents, every key but the
// middle C is off the 12-TET grid by up to half a semitone.
constexpr const char* edo19 = R"(! 19edo.scl
19 equal divisions of the octave
19
!
63.15789
126.31579
189.47368
252.63158
315.78947
378.94737
442.10526
505.26316
568.42105
631.57895
694.73684
757.89474
821.05263
884.21053
947.36842
1010.52632
1073.68421
1136.84211
2/1
)";

// 7-limit just intonation.
constexpr const char* ji7 = R"(! ji7.scl
7-limit just intonation
12
!
15/14
8/7
6/5
5/4
4/3
7/5
3/2
8/5
5/3
7/4
15/8
2/1
)";
constexpr double ji7_ratios[12]
    = {1., 15. / 14., 8. / 7., 6. / 5., 5. / 4., 4. / 3.,
       7. / 5., 3. / 2., 8. / 5., 5. / 3., 7. / 4., 15. / 8.};

// A linear mapping, A at 432 Hz. Its formal octave, 127, is what the
// "128.kbm" key-for-key mappings in circulation hold; a linear mapping has no
// pattern for it to repeat.
constexpr const char* a432 = R"(! a432.kbm
0
0
127
60
69
432.0
127
)";

double edo19_hz(int key)
{
  return middle_c * std::exp2((key - 60) / 19.);
}
double ji7_hz(int key)
{
  const int d = key - 60;
  const int oct = d >= 0 ? d / 12 : -((-d + 11) / 12);
  return middle_c * std::exp2(oct) * ji7_ratios[d - 12 * oct];
}
double tet_hz(int key)
{
  return 440. * std::exp2((key - 69) / 12.);
}
double cents(double f, double ref)
{
  return 1200. * std::log2(f / ref);
}

Process::Inlet* inlet(Process::ProcessModel& p, const QString& name)
{
  for(auto* in : p.inlets())
    if(in->name() == name)
      return in;
  FAIL("no inlet " << name.toStdString() << " on " << p.prettyName().toStdString());
  return nullptr;
}
Process::ControlInlet* control(Process::ProcessModel& p, const QString& name)
{
  auto* c = qobject_cast<Process::ControlInlet*>(inlet(p, name));
  REQUIRE(c);
  return c;
}
Process::Outlet* outlet(Process::ProcessModel& p, const QString& name)
{
  for(auto* out : p.outlets())
    if(out->name() == name)
      return out;
  FAIL("no outlet " << name.toStdString() << " on " << p.prettyName().toStdString());
  return nullptr;
}
int outlet_index(Process::ProcessModel& p, const QString& name)
{
  for(std::size_t i = 0; i < p.outlets().size(); i++)
    if(p.outlets()[i]->name() == name)
      return int(i);
  FAIL("no outlet " << name.toStdString());
  return -1;
}

//! Frequency of the strongest partial between lo and hi, by a Goertzel sweep
//! over a Hann-windowed signal: 0.01 Hz steps, then the parabola through the
//! peak and its two neighbours.
double peak_frequency(const std::vector<double>& x, double rate, double lo, double hi)
{
  const std::size_t n = x.size();
  std::vector<double> w(n);
  for(std::size_t i = 0; i < n; i++)
    w[i] = x[i] * (0.5 - 0.5 * std::cos(2. * M_PI * double(i) / double(n - 1)));

  auto power = [&](double f) {
    const double c = 2. * std::cos(2. * M_PI * f / rate);
    double s1 = 0., s2 = 0.;
    for(std::size_t i = 0; i < n; i++)
    {
      const double s = w[i] + c * s1 - s2;
      s2 = s1;
      s1 = s;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
  };

  constexpr double step = 0.01;
  double best_f = lo, best_p = -1.;
  for(double f = lo; f <= hi; f += step)
  {
    const double p = power(f);
    if(p > best_p)
    {
      best_p = p;
      best_f = f;
    }
  }
  const double a = power(best_f - step), b = best_p, c = power(best_f + step);
  const double denom = a - 2. * b + c;
  return denom != 0. ? best_f + 0.5 * step * (a - c) / denom : best_f;
}

struct played
{
  std::vector<std::vector<float>> frequencies; //!< per tick, what the scaler output
  std::vector<double> audio;                   //!< what the Wavecycle rendered
  std::vector<libremidi::message> midi;        //!< what the scaler's MIDI outlet sent
};

struct chain
{
  score::Document& doc;
  Process::ProcessModel& scaler;
  Process::ProcessModel& wave;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<ossia::graph_node> scaler_node;
  std::shared_ptr<ossia::graph_node> wave_node;
  std::optional<ossia::buffer_tick> ticker;
  int64_t frames{};
  //! The outlet carries UMP: a sysex is a run of SysEx7 packets.
  libremidi::ump_to_midi1_stream<> midi1;

  double rate() const { return plug.context().execState->sampleRate; }
  int buffer() const { return plug.context().execState->bufferSize; }

  void start()
  {
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& base = *plug.baseScenario();
    auto& procs = base.baseInterval().processes();
    scaler_node = procs.find(scaler.id())->second->node;
    wave_node = procs.find(wave.id())->second->node;
    REQUIRE(scaler_node);
    REQUIRE(wave_node);

    auto& itv = *base.baseInterval().OSSIAInterval();
    const ossia::time_value end{int64_t(1e5 * ossia::flicks_per_second<double>)};
    itv.set_min_duration(end);
    itv.set_nominal_duration(end);
    itv.set_max_duration(end);
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    itv.start_and_tick();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    auto& ectx = plug.context();
    ticker.emplace(ossia::buffer_tick{
        *ectx.execState, *ectx.execGraph, base.baseScenario(), {}});
  }

  //! Runs `n` frames with `msgs` on the scaler's MIDI inlet at the first tick.
  played run(int64_t n, std::vector<libremidi::message> msgs = {})
  {
    played p;
    auto* in = scaler_node->root_inputs()[0]->target<ossia::midi_port>();
    REQUIRE(in);
    const int freq_out = outlet_index(scaler, "Frequency output");
    const int midi_out = outlet_index(scaler, "MIDI");

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    for(int64_t i = 0; i < n; i += buffer())
    {
      for(auto& m : msgs)
        in->messages.push_back(libremidi::ump_from_midi1(m));
      msgs.clear();

      (*ticker)(buffer(), double(frames) / rate());
      frames += buffer();
      in->messages.clear();

      auto& fo = *scaler_node->root_outputs()[freq_out]->target<ossia::value_port>();
      std::vector<float> fs;
      for(auto& v : fo.get_data())
      {
        fs.clear();
        if(auto* l = v.value.target<std::vector<ossia::value>>())
          for(auto& e : *l)
            fs.push_back(ossia::convert<float>(e));
      }
      p.frequencies.push_back(std::move(fs));

      auto& mo = *scaler_node->root_outputs()[midi_out]->target<ossia::midi_port>();
      for(auto& u : mo.messages)
      {
        const auto bytes = midi1.feed(u);
        if(!bytes.empty())
        {
          libremidi::message m;
          m.bytes.assign(bytes.begin(), bytes.end());
          p.midi.push_back(std::move(m));
        }
      }

      auto& ao = *wave_node->root_outputs()[0]->target<ossia::audio_port>();
      REQUIRE(ao.channels() > 0);
      const auto& ch = ao.channel(0);
      for(int k = 0; k < buffer(); k++)
        p.audio.push_back(k < int(ch.size()) ? ch[k] : 0.);
    }
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    return p;
  }
};

//! A project folder with the scales in Files/, the scaler reading `scl` from
//! it, cabled to a Wavecycle.
template <typename F>
void with_chain(const char* scl_name, const char* kbm_name, F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_file(dir.path() + "/Files/19edo.scl", edo19);
    score::test::write_file(dir.path() + "/Files/ji7.scl", ji7);
    score::test::write_file(dir.path() + "/Files/a432.kbm", a432);
    auto* doc = score::test::project_document(ctx, dir.path());

    auto* scaler = score::test::add_process(*doc, scaler_uuid, {});
    auto* wave = score::test::add_process(*doc, wavecycle_uuid, {});
    if(!scaler || !wave)
      SKIP("Midi scaler or Wavecycle is not built");

    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
        model, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton,
        *outlet(*scaler, "Frequency output"), *inlet(*wave, "Frequency"));

    control(*scaler, "SCL file")
        ->setValue(std::string("<PROJECT>:Files/") + scl_name);
    if(kbm_name)
      control(*scaler, "KBM file")
          ->setValue(std::string("<PROJECT>:Files/") + kbm_name);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    chain c{*doc, *scaler, *wave, plug, {}, {}, {}, 0, {}};
    c.start();
    f(c);

    plug.clear();
    QApplication::processEvents();
  });
}

//! Pitch the Wavecycle renders over the last second of `p`.
double rendered_pitch(const played& p, double rate, double expected)
{
  const std::size_t n = std::size_t(rate);
  REQUIRE(p.audio.size() > n);
  std::vector<double> tail(p.audio.end() - n, p.audio.end());
  return peak_frequency(tail, rate, expected * 0.95, expected * 1.05);
}
}

TEST_CASE(
    "Midi scaler: notes through the project's .scl come out at its pitches",
    "[midi_scaler][tuning][execution]")
{
  with_chain("19edo.scl", nullptr, [](chain& c) {
    for(int key : {62, 64, 67, 55})
    {
      INFO("key " << key);
      const double expected = edo19_hz(key);
      // Each key is a step of 19-EDO, far enough from 12-TET to tell them apart.
      REQUIRE(std::abs(cents(expected, tet_hz(key))) > 15.);

      auto p = c.run(int64_t(1.5 * c.rate()), {libremidi::channel_events::note_on(1, key, 100)});
      REQUIRE(!p.frequencies.back().empty());
      CHECK(p.frequencies.back().size() == 1);
      const double out = p.frequencies.back().front();
      INFO("scaler output " << out << " Hz, expected " << expected << " Hz");
      CHECK(std::abs(cents(out, expected)) < 0.05);

      const double heard = rendered_pitch(p, c.rate(), expected);
      INFO("rendered " << heard << " Hz");
      CHECK(std::abs(cents(heard, expected)) < 1.);

      c.run(c.buffer(), {libremidi::channel_events::note_off(1, key, 0)});
    }
  });
}

TEST_CASE(
    "Midi scaler: a .scl chosen while playing retunes the next notes",
    "[midi_scaler][tuning][execution]")
{
  with_chain("19edo.scl", nullptr, [](chain& c) {
    c.run(c.buffer());
    control(c.scaler, "SCL file")->setValue(std::string("<PROJECT>:Files/ji7.scl"));
    score::test::run_exec(c.plug);

    for(int key : {64, 70, 58})
    {
      INFO("key " << key);
      const double expected = ji7_hz(key);
      REQUIRE(std::abs(cents(expected, tet_hz(key))) > 10.);

      auto p = c.run(int64_t(1.5 * c.rate()), {libremidi::channel_events::note_on(1, key, 100)});
      REQUIRE(!p.frequencies.back().empty());
      const double out = p.frequencies.back().front();
      INFO("scaler output " << out << " Hz, expected " << expected << " Hz");
      CHECK(std::abs(cents(out, expected)) < 0.05);

      const double heard = rendered_pitch(p, c.rate(), expected);
      INFO("rendered " << heard << " Hz");
      CHECK(std::abs(cents(heard, expected)) < 1.);

      c.run(c.buffer(), {libremidi::channel_events::note_off(1, key, 0)});
    }
  });
}

TEST_CASE(
    "Midi scaler: the MTS output carries the pitches of the project's .scl",
    "[midi_scaler][tuning][mts][execution]")
{
  with_chain("19edo.scl", nullptr, [](chain& c) {
    control(c.scaler, "MIDI output")->setValue(1);
    score::test::run_exec(c.plug);

    auto p = c.run(c.buffer(), {libremidi::channel_events::note_on(1, 64, 100)});

    // The real-time single note tuning changes, keys in order.
    std::vector<int> seen(128, 0);
    bool note_passes = false;
    for(auto& m : p.midi)
    {
      if(m.bytes.size() > 7 && m.bytes[0] == 0xF0 && m.bytes[1] == 0x7F
         && m.bytes[3] == 0x08 && m.bytes[4] == 0x02)
      {
        const int count = m.bytes[6];
        REQUIRE(m.bytes.size() == std::size_t(8 + 4 * count));
        for(int i = 0; i < count; i++)
        {
          const int key = m.bytes[7 + 4 * i];
          const int semi = m.bytes[8 + 4 * i];
          const int frac = (m.bytes[9 + 4 * i] << 7) | m.bytes[10 + 4 * i];
          const double pitch = semi + frac / 16384.;
          const double expected = 69. + 12. * std::log2(edo19_hz(key) / 440.);
          if(expected >= 0. && expected < 127.)
          {
            INFO("key " << key << ": " << pitch << " expected " << expected);
            CHECK(std::abs(pitch - expected) * 100. < 0.01);
          }
          seen[key]++;
        }
      }
      if(m.bytes.size() == 3 && (m.bytes[0] & 0xF0) == 0x90 && m.bytes[1] == 64)
        note_passes = true;
    }
    for(int key = 0; key < 128; key++)
    {
      INFO("key " << key);
      CHECK(seen[key] == 1);
    }
    // The note itself is not moved: the synth retunes the key.
    CHECK(note_passes);
  });
}

TEST_CASE(
    "Midi scaler: the project's .kbm sets the reference of the scale",
    "[midi_scaler][tuning][execution]")
{
  with_chain("19edo.scl", "a432.kbm", [](chain& c) {
    for(int key : {69, 64, 75})
    {
      INFO("key " << key);
      const double expected = 432. * std::exp2((key - 69) / 19.);
      auto p = c.run(int64_t(1.5 * c.rate()), {libremidi::channel_events::note_on(1, key, 100)});
      REQUIRE(!p.frequencies.back().empty());
      const double out = p.frequencies.back().front();
      INFO("scaler output " << out << " Hz, expected " << expected << " Hz");
      CHECK(std::abs(cents(out, expected)) < 0.05);

      const double heard = rendered_pitch(p, c.rate(), expected);
      INFO("rendered " << heard << " Hz");
      CHECK(std::abs(cents(heard, expected)) < 1.);

      c.run(c.buffer(), {libremidi::channel_events::note_off(1, key, 0)});
    }
  });
}
