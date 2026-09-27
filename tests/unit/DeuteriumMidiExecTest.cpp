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

//! Two seconds of a 440 Hz sine, 16 bit mono at 48 kHz.
QString writeSine(const QTemporaryDir& dir)
{
  const QString path = dir.filePath("sine.wav");
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  QDataStream s{&f};
  s.setByteOrder(QDataStream::LittleEndian);
  const quint32 rate = 48000, frames = 2 * rate, bytes = frames * 2;
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
void with_deuterium(F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    const QString wav = writeSine(dir);
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

    r.control(note_off_mode, true);
    r.midi(note_on(1, 60, 100));
    REQUIRE(r.tick() > 0.05);
    r.midi(note_off(1, 60, 0));
    r.tick();
    CHECK(r.tick() > 0.05); // still playing its sample
    // ... to its end (2 s)
    auto& st = *r.plug.context().execState;
    int ticks = 3;
    while(ticks < 1000 && r.tick() > 0.001)
      ticks++;
    const double seconds = double(ticks) * st.bufferSize / st.sampleRate;
    INFO("silent after " << seconds << " s");
    CHECK(seconds > 1.9);
    CHECK(seconds < 2.1);
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
