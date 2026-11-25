// What a synth hears around a MIDI recording: the notes that were playing
// into it when the recording stops end, and the take placed into it plays
// from its first loop. Checked on what arrives at the synth's inlet, through
// taps of the running graph, not on the document.
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/PortTap.hpp>
#include <Process/Process.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/ExecutionContext.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/MidiProcess.hpp>
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <map>
#include <memory>
#include <vector>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
}
template <typename F>
bool spin_until(F f, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

/**
 * What an inlet receives, note by note, with the wall-clock time it was seen
 * at (read every few milliseconds: precise to a tick or two).
 */
struct NoteLog
{
  std::unique_ptr<Execution::PortTap> tap;
  QElapsedTimer clock;
  struct Note
  {
    int pitch{};
    qint64 on{};
    qint64 off{-1};
  };
  std::vector<Note> notes;
  //! Note ons of a pitch already held: a missing note off before them
  int retriggers{};
  //! Note offs of a pitch that was not held
  int strayOffs{};

  NoteLog(const Process::Port& port, const Execution::Context& ctx, const QElapsedTimer& c)
      : tap{std::make_unique<Execution::PortTap>(port, ctx)}
      , clock{c}
  {
  }
  bool linked() const { return tap && tap->monitor(); }
  void read()
  {
    auto* mon = tap ? tap->monitor() : nullptr;
    if(!mon)
      return;
    const qint64 now = clock.elapsed();
    mon->midi.drain([&](const Execution::TapMonitor::Midi& m) {
      if(m.size < 3)
        return;
      const int st = m.bytes[0] & 0xF0;
      const int pitch = m.bytes[1];
      const bool on = st == 0x90 && m.bytes[2] > 0;
      const bool off = st == 0x80 || (st == 0x90 && m.bytes[2] == 0);
      if(on)
      {
        for(auto& n : notes)
          if(n.pitch == pitch && n.off < 0)
          {
            retriggers++;
            n.off = now;
          }
        notes.push_back({pitch, now, -1});
      }
      else if(off)
      {
        bool found = false;
        for(auto& n : notes)
          if(n.pitch == pitch && n.off < 0)
          {
            n.off = now;
            found = true;
          }
        if(!found)
          strayOffs++;
      }
    });
  }
  //! Notes held longer than `ms`, or still held after it
  int heldLongerThan(qint64 ms) const
  {
    const qint64 now = clock.elapsed();
    int r = 0;
    for(auto& n : notes)
      if((n.off < 0 ? now : n.off) - n.on > ms)
        r++;
    return r;
  }
  int onsBetween(qint64 from, qint64 to) const
  {
    int r = 0;
    for(auto& n : notes)
      if(n.on >= from && n.on < to)
        r++;
    return r;
  }
};

void spinReading(int ms, std::vector<NoteLog*> logs)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
    for(auto* l : logs)
      l->read();
  }
}

UuidKey<Process::ProcessModel> processKey(
    const score::GUIApplicationContext& ctx, const QString& name)
{
  for(auto& f : ctx.interfaces<Process::ProcessFactoryList>())
    if(f.prettyName() == name)
      return f.concreteKey();
  return {};
}

struct Setup
{
  score::Document* doc{};
  Midi::ProcessModel* source{};
  Process::ProcessModel* synth{};
};

//! A piano roll playing a note held for most of every quarter, cabled into
//! a process with a MIDI inlet standing for a synth, in the root box
Setup makeSetup(const score::GUIApplicationContext& ctx)
{
  Setup s;
  s.doc = score::test::new_document(ctx);
  REQUIRE(s.doc);
  auto& root = score::test::base_interval(*s.doc);
  auto& dm = static_cast<Scenario::ScenarioDocumentModel&>(s.doc->model().modelDelegate());
  const auto synth_key = processKey(ctx, "Midi scale");
  if(synth_key == UuidKey<Process::ProcessModel>{})
    SKIP("no Midi scale process in this build");
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, s.doc->context()};
  s.source = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
      root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
  REQUIRE(s.source);
  std::vector<Midi::NoteData> notes;
  const int quarters = int(s.source->duration().impl / 352800000);
  for(int i = 0; i < quarters; i++)
    notes.emplace_back(double(i) / quarters, 0.9 / quarters, 60 + (i % 4), 100);
  m.submit(new Midi::ReplaceNotes{*s.source, notes, 0, 127, s.source->duration()});
  s.synth = m.createProcessInNewSlot(root, synth_key, {});
  REQUIRE(s.synth);
  REQUIRE(!s.synth->inlets().empty());
  REQUIRE(s.synth->inlets()[0]->type() == Process::PortType::Midi);
  m.createCable(
      dm, *s.source->outlets()[0], *s.synth->inlets()[0],
      Process::CableType::ImmediateGlutton);
  m.commit();
  return s;
}

bool playSteadily(const score::GUIApplicationContext& ctx, score::Document& doc)
{
  auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  auto& exec = doc.context().plugin<Execution::DocumentPlugin>();
  // The first playback can be stopped by the audio engine starting
  for(int i = 0; i < 5; i++)
  {
    if(!exec.isPlaying())
      engine.execution().request_play_global(true);
    spin(1500);
    if(exec.isPlaying())
      return true;
  }
  return false;
}

// At 120: a quarter is 500 ms, a bar 2 s; the notes last 450 ms
constexpr qint64 noteMs = 450;
constexpr qint64 slackMs = 250;

void recordSynthInlet(
    const score::GUIApplicationContext& ctx, Setup& s, bool muteSource, int bars)
{
  auto& exec = s.doc->context().plugin<Execution::DocumentPlugin>();
  auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  REQUIRE(playSteadily(ctx, *s.doc));
  QElapsedTimer clock;
  clock.start();

  // What the synth gets from the source, from before the recording
  NoteLog before{*s.synth->inlets()[0], exec.context(), clock};
  REQUIRE(before.linked());

  auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
  auto opt = rec.defaultOptions();
  opt.startRate = 1.;
  opt.lengthBars = bars;
  opt.loopFixedLength = true;
  opt.muteSource = muteSource;
  opt.target = Recording::RecordOptions::Target::ThisBox;
  auto* session = rec.startSession(s.doc->context(), {s.synth->inlets()[0]}, opt);
  REQUIRE(session);
  bool finished = false;
  QPointer<Process::ProcessModel> take;
  QObject::connect(session, &Recording::RecordSession::finished, [&] {
    finished = true;
    if(!session->takes().empty())
      take = session->takes()[0].placed;
  });
  // The next bar, then the bars: it ends by itself
  QElapsedTimer t;
  t.start();
  while(!finished && t.elapsed() < (bars + 2) * 2000 + 2000)
    spinReading(20, {&before});
  REQUIRE(finished);
  REQUIRE(take);
  const qint64 placedAt = clock.elapsed();

  // What the synth gets once the take is cabled into it, and what the take
  // sends: the take repeats the source's notes, which overlap when both play
  spin(100);
  const qint64 linkedAt = clock.elapsed();
  NoteLog after{*s.synth->inlets()[0], exec.context(), clock};
  NoteLog out{*take->outlets()[0], exec.context(), clock};
  REQUIRE(after.linked());
  REQUIRE(out.linked());
  CHECK(!take->muted());
  spinReading(bars * 2000 + 2500, {&before, &after, &out});

  INFO("mute source: " << muteSource << ", bars: " << bars);
  // Every note the source played ends, the one held when the take was
  // placed included, and every note of the take
  CHECK(before.retriggers == 0);
  CHECK(before.heldLongerThan(noteMs + slackMs) == 0);
  CHECK(out.retriggers == 0);
  CHECK(out.heldLongerThan(noteMs + slackMs) == 0);
  if(muteSource)
  {
    // Only the take plays into the synth
    CHECK(after.retriggers == 0);
    CHECK(after.heldLongerThan(noteMs + slackMs) == 0);
  }

  // The take plays at once, from where its box is in its loop: a note every
  // quarter
  REQUIRE(!out.notes.empty());
  INFO("first note of the take " << out.notes.front().on - linkedAt << " ms after it was placed");
  CHECK(out.notes.front().on - linkedAt < 500 + slackMs);
  CHECK(out.onsBetween(linkedAt, linkedAt + 2000) >= 3);
  CHECK(after.onsBetween(linkedAt, linkedAt + 2000) >= (muteSource ? 3 : 6));

  engine.execution().request_stop();
  spin(300);
}
}

TEST_CASE("A MIDI recording of a synth's inlet leaves no note stuck in it, source muted", "[recording][midi-flow][stuck]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto s = makeSetup(ctx);
    recordSynthInlet(ctx, s, true, 1);
  });
}

TEST_CASE("A MIDI recording of a synth's inlet leaves no note stuck in it, source playing", "[recording][midi-flow][stuck]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto s = makeSetup(ctx);
    recordSynthInlet(ctx, s, false, 1);
  });
}

TEST_CASE("Four bars of MIDI recorded into a synth play from their first loop", "[recording][midi-flow][first-loop]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto s = makeSetup(ctx);
    recordSynthInlet(ctx, s, true, 4);
  });
}

TEST_CASE("A piano roll added to a box while it plays is heard at once", "[recording][midi-flow][added-live]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    for(bool loop : {false, true})
    {
      auto* doc = score::test::new_document(ctx);
      REQUIRE(doc);
      auto& root = score::test::base_interval(*doc);
      REQUIRE(playSteadily(ctx, *doc));
      // Past the first bar: a loop of a bar plays from its middle
      spin(2700);
      auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
      Midi::ProcessModel* roll{};
      {
        Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
        roll = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
            root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
        REQUIRE(roll);
        // A note every quarter of the first bar only: what a loop of a bar plays
        std::vector<Midi::NoteData> notes;
        const double quarters = double(roll->duration().impl) / 352800000.;
        for(int i = 0; i < 4; i++)
          notes.emplace_back(double(i) / quarters, 0.5 / quarters, 60, 100);
        if(!loop)
          for(int i = 4; i < int(quarters); i++)
            notes.emplace_back(double(i) / quarters, 0.5 / quarters, 60, 100);
        m.submit(new Midi::ReplaceNotes{*roll, notes, 0, 127, roll->duration()});
        if(loop)
        {
          m.submit(new Process::SetLoop{*roll, true});
          m.submit(new Process::SetLoopDuration{*roll, TimeVal{2 * 705600000LL}});
        }
        m.commit();
      }
      spin(300);
      QElapsedTimer clock;
      clock.start();
      NoteLog out{*roll->outlets()[0], exec.context(), clock};
      REQUIRE(out.linked());
      spinReading(1500, {&out});
      INFO("loop: " << loop);
      // A note every quarter from the first one
      CHECK(out.onsBetween(0, 1500) >= 2);
      ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution().request_stop();
      spin(500);
    }
  });
}

namespace
{
//! Records the synth's inlet from now, and stops the recording by hand while
//! a note is held into the synth
void stopWhileHeld(const score::GUIApplicationContext& ctx, Setup& s, bool muteSource)
{
  auto& exec = s.doc->context().plugin<Execution::DocumentPlugin>();
  auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  REQUIRE(playSteadily(ctx, *s.doc));
  QElapsedTimer clock;
  clock.start();
  NoteLog before{*s.synth->inlets()[0], exec.context(), clock};
  REQUIRE(before.linked());
  // What the synth plays: what it got, whatever the graph does to its inlet
  NoteLog heard{*s.synth->outlets()[0], exec.context(), clock};
  REQUIRE(heard.linked());

  auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
  auto opt = rec.defaultOptions();
  opt.start = Recording::RecordOptions::Start::Now;
  opt.startRate = 0.;
  opt.lengthBars = 0.;
  opt.muteSource = muteSource;
  opt.target = Recording::RecordOptions::Target::ThisBox;
  auto* session = rec.startSession(s.doc->context(), {s.synth->inlets()[0]}, opt);
  REQUIRE(session);
  bool finished = false;
  QPointer<Process::ProcessModel> take;
  QObject::connect(session, &Recording::RecordSession::finished, [&] {
    finished = true;
    if(!session->takes().empty())
      take = session->takes()[0].placed;
  });
  spinReading(2200, {&before, &heard});
  // A note just started: stopped 100 ms into it
  const auto held = [&] {
    for(auto& n : before.notes)
      if(n.off < 0 && clock.elapsed() - n.on > 100)
        return true;
    return false;
  };
  QElapsedTimer t;
  t.start();
  while(!held() && t.elapsed() < 2000)
    spinReading(2, {&before, &heard});
  REQUIRE(held());
  const qint64 stoppedAt = clock.elapsed();
  rec.stopSession();
  while(!finished && t.elapsed() < 6000)
    spinReading(10, {&before, &heard});
  REQUIRE(finished);
  REQUIRE(take);
  spin(100);
  const qint64 linkedAt = clock.elapsed();
  NoteLog after{*s.synth->inlets()[0], exec.context(), clock};
  NoteLog out{*take->outlets()[0], exec.context(), clock};
  spinReading(3000, {&before, &after, &out, &heard});
  INFO("mute source: " << muteSource);
  CHECK(heard.heldLongerThan(noteMs + slackMs) == 0);
  CHECK(before.retriggers == 0);
  CHECK(before.heldLongerThan(noteMs + slackMs) == 0);
  CHECK(out.retriggers == 0);
  CHECK(out.heldLongerThan(noteMs + slackMs) == 0);
  if(muteSource)
    CHECK(after.heldLongerThan(noteMs + slackMs) == 0);
  (void)linkedAt;
  // Playback stopping is not looked at: a tap goes with the execution, before
  // its last tick
  engine.execution().request_stop();
  spin(300);
}
}

TEST_CASE("A MIDI recording stopped by hand while a note is held leaves it ending, source muted", "[recording][midi-flow][stuck][by-hand]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto s = makeSetup(ctx);
    stopWhileHeld(ctx, s, true);
  });
}

TEST_CASE("A MIDI recording stopped by hand while a note is held leaves it ending, source playing", "[recording][midi-flow][stuck][by-hand]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto s = makeSetup(ctx);
    stopWhileHeld(ctx, s, false);
  });
}
