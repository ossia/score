// A MIDI file's ticks count quarters and its tempo says how fast they go.
// score's time is musical time where a quarter lasts half a second, so a file
// is placed by its quarters whatever its tempo, and a looper's take keeps the
// tempo it was played at.

#include <Process/Drop/ClipMetadata.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <Fx/Looper/MidiTake.hpp>
#include <Midi/MidiDrop.hpp>
#include <Midi/MidiNote.hpp>
#include <Midi/MidiProcess.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/reader.hpp>
#include <libremidi/writer.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <vector>

using Catch::Approx;

namespace
{
Nodes::Looper::midi_note looped_note(double start, double length, int pitch)
{
  Nodes::Looper::midi_note n{};
  n.start = int64_t(std::llround(start));
  n.length = int64_t(std::llround(length));
  n.channel = 1;
  n.pitch = uint8_t(pitch);
  n.velocity = 100;
  return n;
}

//! Two bars of 4/4 at `bpm`: a quarter on the first beat, a half note on the
//! second bar's first beat, a quarter on its last beat.
QByteArray two_bars(double bpm)
{
  libremidi::writer w;
  w.ticksPerQuarterNote = 480;
  struct ev
  {
    int tick;
    libremidi::message m;
  };
  const ev events[]{
      {0, libremidi::meta_events::tempo(int(std::lround(60'000'000. / bpm)))},
      {0, libremidi::meta_events::time_signature(4, 4, 24, 8)},
      {0, libremidi::channel_events::note_on(1, 60, 100)},
      {480, libremidi::channel_events::note_off(1, 60, 0)},
      {4 * 480, libremidi::channel_events::note_on(1, 62, 100)},
      {6 * 480, libremidi::channel_events::note_off(1, 62, 0)},
      {7 * 480, libremidi::channel_events::note_on(1, 64, 100)},
      {8 * 480, libremidi::channel_events::note_off(1, 64, 0)},
  };
  int prev = 0;
  for(const auto& e : events)
  {
    w.add_event(e.tick - prev, 0, e.m);
    prev = e.tick;
  }
  std::ostringstream out;
  w.write(out);
  const auto s = std::move(out).str();
  return QByteArray(s.data(), qsizetype(s.size()));
}

Scenario::ProcessModel* main_scenario(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto s = qobject_cast<Scenario::ProcessModel*>(&p))
      return s;
  return nullptr;
}

struct placed_note
{
  double start_ms;
  double duration_ms;
  int pitch;
};

struct placed_file
{
  double duration_ms{};
  std::vector<placed_note> notes;
};

//! `file` dropped in the main scenario as a drop of it in an empty spot does:
//! a new box as long as the drop says, the drop's process in it.
placed_file drop(score::Document& doc, const QByteArray& file)
{
  placed_file res;
  auto* scenario = main_scenario(doc);
  REQUIRE(scenario);

  QMimeData mime;
  mime.setData("audio/midi", file);
  const auto& handlers
      = doc.context().app.interfaces<Process::ProcessDropHandlerList>();
  auto drops = handlers.getDrop(mime, doc.context());
  REQUIRE(drops.size() == 1);
  REQUIRE(drops[0].duration);
  const TimeVal duration = *drops[0].duration;
  res.duration_ms = duration.msec();

  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  auto& box = m.createBox(*scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(1000) + duration, 0.4);
  auto* p = m.createProcessInNewSlot(box, drops[0].creation);
  REQUIRE(p);
  score::Dispatcher_T disp{m};
  drops[0].setup(*p, disp);
  m.commit();

  auto* midi = qobject_cast<Midi::ProcessModel*>(p);
  REQUIRE(midi);
  const double process_ms = midi->duration().msec();
  for(const auto& n : midi->notes)
  {
    const auto d = n.noteData();
    res.notes.push_back({d.start() * process_ms, d.duration() * process_ms, int(d.pitch())});
  }
  std::sort(res.notes.begin(), res.notes.end(), [](const auto& a, const auto& b) {
    return a.start_ms < b.start_ms;
  });
  return res;
}
}

TEST_CASE("A MIDI take is written at the tempo and in the meter it was played in",
          "[fx][midi][looper][takes]")
{
  Nodes::Looper::midi_export e;
  e.rate = 48000.;
  e.tempo = 90.;
  e.meter_numerator = 3;
  e.meter_denominator = 4;
  // Four bars of 3/4 at 90 bpm: 12 quarters.
  const double fpq = 48000. * 60. / 90.;
  e.length = int64_t(std::llround(12 * fpq));
  e.notes[0] = looped_note(0, fpq, 60);
  e.notes[1] = looped_note(3 * fpq, 1.5 * fpq, 62);
  e.note_count = 2;

  const auto bytes = e.to_smf();
  libremidi::reader r;
  std::vector<uint8_t> raw(bytes.begin(), bytes.end());
  REQUIRE(r.parse(raw) != libremidi::reader::invalid);
  REQUIRE(r.tracks.size() == 1);
  CHECK(r.ticksPerBeat == Approx(480.));
  // The ticks count quarters, not seconds at some tempo.
  CHECK(r.get_end_time() == Approx(12. * 480.));

  int tick = 0;
  bool tempo_seen = false, meter_seen = false;
  std::vector<std::pair<int, int>> ons, offs;
  for(const auto& ev : r.tracks[0])
  {
    tick += ev.tick;
    if(ev.m.is_meta_event() && ev.m.get_meta_event_type() == libremidi::meta_event_type::TEMPO_CHANGE)
    {
      tempo_seen = true;
      CHECK(tick == 0);
      REQUIRE(ev.m.size() == 6);
      const int us = (ev.m.bytes[3] << 16) | (ev.m.bytes[4] << 8) | ev.m.bytes[5];
      CHECK(us == 666667);
    }
    else if(ev.m.is_meta_event() && ev.m.get_meta_event_type() == libremidi::meta_event_type::TIME_SIGNATURE)
    {
      meter_seen = true;
      CHECK(tick == 0);
      REQUIRE(ev.m.size() == 7);
      CHECK(ev.m.bytes[3] == 3);
      CHECK(ev.m.bytes[4] == 2); // a quarter: 2^2
    }
    else if(ev.m.get_message_type() == libremidi::message_type::NOTE_ON && ev.m.bytes[2] > 0)
      ons.push_back({tick, ev.m.bytes[1]});
    else if(ev.m.get_message_type() == libremidi::message_type::NOTE_OFF
            || ev.m.get_message_type() == libremidi::message_type::NOTE_ON)
      offs.push_back({tick, ev.m.bytes[1]});
  }
  CHECK(tempo_seen);
  CHECK(meter_seen);
  REQUIRE(ons.size() == 2);
  CHECK(ons[0] == std::pair{0, 60});
  CHECK(ons[1] == std::pair{3 * 480, 62});
  REQUIRE(offs.size() == 2);
  CHECK(offs[0] == std::pair{480, 60});
  CHECK(offs[1] == std::pair{int(4.5 * 480), 62});
}

TEST_CASE("A MIDI take at 120 bpm says so", "[fx][midi][looper][takes]")
{
  Nodes::Looper::midi_export e;
  e.rate = 44100.;
  e.tempo = 120.;
  e.length = 4 * 22050;
  e.notes[0] = looped_note(0, 22050, 60);
  e.note_count = 1;
  const auto bytes = e.to_smf();
  libremidi::reader r;
  std::vector<uint8_t> raw(bytes.begin(), bytes.end());
  REQUIRE(r.parse(raw) != libremidi::reader::invalid);
  REQUIRE(r.tracks.size() == 1);
  CHECK(r.get_end_time() == Approx(4. * 480.));
  int us = 0;
  for(const auto& ev : r.tracks[0])
    if(ev.m.is_meta_event() && ev.m.get_meta_event_type() == libremidi::meta_event_type::TEMPO_CHANGE)
      us = (ev.m.bytes[3] << 16) | (ev.m.bytes[4] << 8) | ev.m.bytes[5];
  CHECK(us == 500000);
}

TEST_CASE("A MIDI file is placed by its quarters, whatever its tempo",
          "[midi][drop][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // Parsed alone: 8 quarters are 4 s of score's time at any tempo.
    for(double bpm : {60., 90., 120., 174.})
    {
      INFO(bpm << " bpm");
      const auto song = Midi::MidiTrack::parse(two_bars(bpm), doc->context());
      CHECK(song.durationInMs == Approx(4000.));
      REQUIRE(song.tracks.size() == 1);
      CHECK(song.tracks[0].notes.size() == 3);
    }

    // Dropped: the same box and the same notes at 90 and at 120 bpm.
    const auto at_90 = drop(*doc, two_bars(90.));
    const auto at_120 = drop(*doc, two_bars(120.));
    CHECK(at_90.duration_ms == Approx(4000.).margin(1.));
    CHECK(at_120.duration_ms == Approx(at_90.duration_ms).margin(1.));

    const placed_note expected[]{{0., 500., 60}, {2000., 1000., 62}, {3500., 500., 64}};
    for(const auto* placed : {&at_90, &at_120})
    {
      REQUIRE(placed->notes.size() == 3);
      for(int i = 0; i < 3; i++)
      {
        INFO("note " << i);
        CHECK(placed->notes[i].start_ms == Approx(expected[i].start_ms).margin(1.));
        CHECK(placed->notes[i].duration_ms == Approx(expected[i].duration_ms).margin(1.));
        CHECK(placed->notes[i].pitch == expected[i].pitch);
      }
    }
  });
}

TEST_CASE("A looper take at 90 bpm is placed by its quarters",
          "[midi][drop][looper][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // Two bars at 90 bpm: 8 quarters, 4 s of score's time.
    Nodes::Looper::midi_export e;
    e.rate = 48000.;
    e.tempo = 90.;
    const double fpq = 48000. * 60. / 90.;
    e.length = int64_t(std::llround(8 * fpq));
    e.notes[0] = looped_note(4 * fpq, 2 * fpq, 62);
    e.note_count = 1;
    const auto bytes = e.to_smf();

    const auto placed = drop(*doc, QByteArray(bytes.data(), qsizetype(bytes.size())));
    CHECK(placed.duration_ms == Approx(4000.).margin(1.));
    REQUIRE(placed.notes.size() == 1);
    CHECK(placed.notes[0].start_ms == Approx(2000.).margin(1.));
    CHECK(placed.notes[0].duration_ms == Approx(1000.).margin(1.));
  });
}

TEST_CASE("A MIDI file from a file browser is not looped; a looper's take loops at its length",
          "[midi][drop][looper][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);

    QTemporaryDir dir;
    const QString path = dir.path() + "/song.mid";
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(two_bars(90.));
    }
    auto mime_of = [&](bool take) {
      auto mime = std::make_unique<QMimeData>();
      mime->setUrls({QUrl::fromLocalFile(path)});
      if(take)
      {
        const auto meta = Process::encode_clip_metadata(
            {path.toStdString()}, halp::clip_stretch::automatic, true);
        mime->setData(
            QString::fromUtf8(Process::clip_metadata_mime.data()),
            QByteArray(meta.data(), qsizetype(meta.size())));
      }
      return mime;
    };

    // In a new box as long as the drop says, or in a box of 10 s already there.
    auto place = [&](const QMimeData& mime, bool in_long_box) -> Midi::ProcessModel* {
      const auto& handlers = ctx.interfaces<Process::ProcessDropHandlerList>();
      auto drops = handlers.getDrop(mime, doc->context());
      REQUIRE(drops.size() == 1);
      REQUIRE(drops[0].duration);
      const TimeVal length = in_long_box ? TimeVal::fromMsecs(10000) : *drops[0].duration;
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& box = m.createBox(
          *scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(1000) + length, 0.4);
      auto* p = m.createProcessInNewSlot(box, drops[0].creation);
      REQUIRE(p);
      score::Dispatcher_T disp{m};
      drops[0].setup(*p, disp);
      m.commit();
      return qobject_cast<Midi::ProcessModel*>(p);
    };
    auto starts_ms = [](const Midi::ProcessModel& midi) {
      std::vector<double> res;
      for(const auto& n : midi.notes)
        res.push_back(n.start() * midi.duration().msec());
      std::sort(res.begin(), res.end());
      return res;
    };
    auto near = [](const std::vector<double>& a, const std::vector<double>& b) {
      if(a.size() != b.size())
        return false;
      for(std::size_t i = 0; i < a.size(); i++)
        if(std::abs(a[i] - b[i]) > 1.)
          return false;
      return true;
    };
    const std::vector<double> expected{0., 2000., 3500.};

    for(const bool in_long_box : {false, true})
    {
      INFO((in_long_box ? "in a box of 10 s" : "in a new box"));
      {
        INFO("a file");
        auto* midi = place(*mime_of(false), in_long_box);
        REQUIRE(midi);
        CHECK(!midi->loops());
        CHECK(midi->duration().msec() == Approx(in_long_box ? 10000. : 4000.).margin(1.));
        CHECK(near(starts_ms(*midi), expected));
      }
      {
        INFO("a take");
        auto* midi = place(*mime_of(true), in_long_box);
        REQUIRE(midi);
        CHECK(midi->loops());
        CHECK(midi->loopDuration().msec() == Approx(4000.).margin(1.));
        CHECK(midi->duration().msec() == Approx(in_long_box ? 10000. : 4000.).margin(1.));
        CHECK(near(starts_ms(*midi), expected));
      }
    }
  });
}
