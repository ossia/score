// Deuterium's MIDI handling through the real execution node: the "Note off"
// control (Ignore plays a note to the end of its sample) and the MIDI channel
// filter.

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/Process.hpp>

#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <libremidi/ump_events.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <cmath>

namespace
{
const QString deuterium_uuid = QStringLiteral("95f8ee65-e418-4f75-b5e4-3e039bb90ac8");
const QString envelope_source = QStringLiteral("Envelope source");
const QString release = QStringLiteral("Release");
const QString note_off_mode = QStringLiteral("Note off");
const QString midi_channel = QStringLiteral("MIDI channel");

//! `seconds` of a 440 Hz sine, 16 bit mono at 48 kHz.
QString writeSine(const QTemporaryDir& dir, double seconds = 2.)
{
  const QString path = dir.filePath("sine.wav");
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  QDataStream s{&f};
  s.setByteOrder(QDataStream::LittleEndian);
  const quint32 rate = 48000, frames = quint32(seconds * rate), bytes = frames * 2;
  f.write("RIFF");
  s << quint32(36 + bytes);
  f.write("WAVEfmt ");
  s << quint32(16) << quint16(1) << quint16(1) << rate << quint32(rate * 2) << quint16(2)
    << quint16(16);
  f.write("data");
  s << bytes;
  for(quint32 i = 0; i < frames; i++)
    s << qint16(16000 * std::sin(2. * M_PI * 440. * i / rate));
  return path;
}

struct rig
{
  Process::ProcessModel& proc;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<ossia::graph_node> node;
  int64_t date{};

  void control(const QString& name, const ossia::value& v)
  {
    Process::ControlInlet* c{};
    for(auto* in : proc.inlets())
      if(in->name() == name)
        c = qobject_cast<Process::ControlInlet*>(in);
    INFO(name.toStdString());
    REQUIRE(c);
    c->setValue(v);
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  }

  void midi(const libremidi::ump& m)
  {
    auto& port = *node->root_inputs()[0]->target<ossia::midi_port>();
    port.messages.push_back(m);
  }

  //! Runs one buffer; the RMS of the left channel.
  double tick()
  {
    const int frames = plug.context().execState->bufferSize;
    const double rate = plug.context().execState->sampleRate;
    auto flicks = [&](int64_t s) {
      return ossia::time_value{int64_t(s * ossia::flicks_per_second<double> / rate)};
    };
    ossia::token_request tk;
    tk.prev_date = flicks(date);
    tk.date = flicks(date + frames);
    tk.start_sample = 0;
    tk.length_sample = frames;
    date += frames;
    // A new graph tick, as the graph counts them: the node keeps track of
    // what it already consumed within one.
    plug.context().execState->samples_since_start += frames;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node->run(tk, ossia::exec_state_facade{plug.context().execState.get()});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    node->root_inputs()[0]->target<ossia::midi_port>()->messages.clear();
    auto& out = *node->root_outputs()[0]->target<ossia::audio_port>();
    double sum = 0.;
    int n = 0;
    if(out.channels() > 0)
    {
      auto& ch = out.channel(0);
      for(int i = 0; i < frames && i < int(ch.size()); i++, n++)
        sum += ch[i] * ch[i];
      for(auto& c : out.get())
        std::fill(c.begin(), c.end(), 0.);
    }
    return n ? std::sqrt(sum / n) : 0.;
  }
};

template <typename F>
void with_deuterium(F&& f, double sampleSeconds = 2.)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    const QString wav = writeSine(dir, sampleSeconds);
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, deuterium_uuid, wav);
    if(!proc)
      SKIP("Deuterium not built");

    // The file loads asynchronously; a two-second wav takes a moment.
    QElapsedTimer t;
    t.start();
    while(t.elapsed() < 1500)
      QApplication::processEvents(QEventLoop::AllEvents, 10);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    rig r{*proc, plug, it->second->node};
    // A short release of our own, so that a released note is quickly silent
    r.control(envelope_source, false);
    r.control(release, ossia::vec2f{0.01f, 0.f});
    f(r);
    QApplication::processEvents();
  });
}

using libremidi::from_midi1::note_off;
using libremidi::from_midi1::note_on;
}

TEST_CASE("Deuterium: a note-off releases, unless note-offs are ignored", "[deuterium][execution]")
{
  with_deuterium([](rig& r) {
    r.midi(note_on(1, 60, 100));
    REQUIRE(r.tick() > 0.05); // the sample sounds
    r.midi(note_off(1, 60, 0));
    r.tick();
    CHECK(r.tick() < 0.001); // released

    // Ignored: the note-off does nothing; the envelope goes into its own
    // release (3 s to -100 dB) after the decay
    r.control(note_off_mode, true);
    r.control(release, ossia::vec2f{3.f, 0.f});
    r.midi(note_on(1, 60, 100));
    REQUIRE(r.tick() > 0.05);
    r.midi(note_off(1, 60, 0));
    r.tick();
    CHECK(r.tick() > 0.05); // still playing
    // ... until the release fades it: -43 dB, the threshold below, at 1.3 s
    auto& st = *r.plug.context().execState;
    int ticks = 3;
    while(ticks < 1000 && r.tick() > 0.001)
      ticks++;
    const double seconds = double(ticks) * st.bufferSize / st.sampleRate;
    INFO("silent after " << seconds << " s");
    CHECK(seconds > 1.1);
    CHECK(seconds < 1.6);
  });
}

TEST_CASE("Deuterium: the MIDI channel filter", "[deuterium][execution]")
{
  with_deuterium([](rig& r) {
    r.control(midi_channel, 2);
    r.midi(note_on(0, 60, 100)); // channel 1 (0-based on the wire)
    CHECK(r.tick() < 0.001);
    r.midi(note_on(1, 62, 100)); // channel 2
    CHECK(r.tick() > 0.05);

    // All: any channel
    r.midi(note_off(1, 62, 0));
    r.tick();
    r.tick();
    r.control(midi_channel, 0);
    r.midi(note_on(9, 64, 100));
    CHECK(r.tick() > 0.05);
  });
}

// Ignore must not make a note a one-shot, which does not loop: a sampled piano
// is a short attack and a loop, faded out by its envelope. The loop plays on
// and the envelope ends the note; a new strike of the key releases the last.
TEST_CASE("Deuterium: ignored note-offs keep the sample's loop", "[deuterium][execution]")
{
  with_deuterium([](rig& r) {
    // A 0.1 s sample looping, fading out over a 1 s decay, like a piano
    r.control(QStringLiteral("Loop"), 2); // Forward
    r.control(QStringLiteral("Sustain"), 0.f);
    r.control(QStringLiteral("Decay"), ossia::vec2f{1.f, 0.f});
    r.control(note_off_mode, true);
    auto& st = *r.plug.context().execState;

    r.midi(note_on(1, 60, 100));
    r.tick();
    r.midi(note_off(1, 60, 0));
    int ticks = 1;
    std::string curve;
    for(double v; ticks < 1000 && (v = r.tick()) > 0.0005; ticks++)
      if(ticks % 4 == 0)
        curve += std::to_string(v) + " ";
    const double seconds = double(ticks) * st.bufferSize / st.sampleRate;
    INFO("silent after " << seconds << " s: " << curve << " rate " << st.sampleRate << " buffer " << st.bufferSize);
    // The 1 s decay reaches -50 dB half way, not at 0.1 s, the end of the sample
    CHECK(seconds > 0.3);
    CHECK(seconds < 1.5);

    // A sustaining envelope still ends: the release follows the decay, and
    // takes the release time (0.5 s to -100 dB here: -43 dB at 0.22 s),
    // whenever the note-off came.
    r.control(QStringLiteral("Sustain"), 1.f);
    r.control(QStringLiteral("Decay"), ossia::vec2f{0.05f, 0.f});
    r.control(release, ossia::vec2f{0.5f, 0.f});
    r.midi(note_on(1, 60, 100));
    r.tick();
    r.midi(note_off(1, 60, 0));
    ticks = 1;
    while(ticks < 1000 && r.tick() > 0.0005)
      ticks++;
    const double sustained = double(ticks) * st.bufferSize / st.sampleRate;
    INFO("sustaining note silent after " << sustained << " s");
    CHECK(sustained > 0.15);
    CHECK(sustained < 0.4);

  }, 0.1);
}
TEST_CASE("SCRATCH loop held", "[deuterium][scratch]")
{
  for(int ign : {0, 1})
  with_deuterium([ign](rig& r) {
    r.control(QStringLiteral("Loop"), 2);
    r.control(QStringLiteral("Sustain"), ign == 2 ? 0.f : 1.f);
    r.control(note_off_mode, bool(ign));
    auto& st = *r.plug.context().execState;
    r.midi(note_on(1, 60, 100));
    int ticks = 1; std::string curve;
    for(int i = 0; i < 60; i++) { double v = r.tick(); if(i > 30 && i < 45) curve += std::to_string(v) + " "; }
    WARN("ignore " << ign << " held (sustain 1) silent after " << double(ticks) * st.bufferSize / st.sampleRate << ": " << curve);
  }, 0.1);
}

// Each note keeps the note-off mode it started in, so a note held while the
// mode switches between Release and Ignore does not get stuck.
TEST_CASE("Deuterium: a note keeps the note-off mode it started with", "[deuterium][execution]")
{
  with_deuterium([](rig& r) {
    // Started in Release mode, released after the switch to Ignore: stops
    r.midi(note_on(1, 60, 100));
    REQUIRE(r.tick() > 0.05);
    r.control(note_off_mode, true);
    r.midi(note_off(1, 60, 0));
    r.tick();
    CHECK(r.tick() < 0.001);

    // Started in Ignore mode, back to Release before the note-off: the note
    // still ends by its own envelope (a sustain of 1 and a 0.2 s release)
    r.control(release, ossia::vec2f{0.2f, 0.f});
    r.midi(note_on(1, 62, 100));
    REQUIRE(r.tick() > 0.05);
    r.control(note_off_mode, false);
    r.midi(note_off(1, 62, 0));
    auto& st = *r.plug.context().execState;
    int ticks = 1;
    while(ticks < 1000 && r.tick() > 0.0005)
      ticks++;
    const double seconds = double(ticks) * st.bufferSize / st.sampleRate;
    INFO("silent after " << seconds << " s");
    CHECK(seconds < 0.5);
  });
}
