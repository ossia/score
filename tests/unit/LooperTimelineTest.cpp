// A looper's take sent to the timeline becomes a box of the scenario that
// holds the looper, at the date the take was recorded, in one undoable step.

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/write_loop_wav.hpp>

#include <QApplication>
#include <QTemporaryDir>

#include <Crousti/HostServices.hpp>
#include <Midi/MidiProcess.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Fx/Looper/MidiTake.hpp>

#include <cmath>
#include <fstream>
#include <vector>

using Catch::Approx;

namespace
{
const QString audio_looper_uuid = QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
const QString midi_looper_uuid = QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");

Scenario::ProcessModel* main_scenario(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto s = qobject_cast<Scenario::ProcessModel*>(&p))
      return s;
  return nullptr;
}

//! A box from 2 s to 10 s of the main scenario, with a looper in it.
Process::ProcessModel*
add_looper(score::Document& doc, Scenario::ProcessModel& scenario, const QString& uuid)
{
  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  auto& box = m.createBox(
      scenario, TimeVal::fromMsecs(2000), TimeVal::fromMsecs(10000), 0.2);
  auto* p = m.createProcessInNewSlot(
      box, UuidKey<Process::ProcessModel>::fromString(uuid), QString{});
  m.commit();
  return p;
}

//! Whether an interval goes from the scenario's start to `date`: what makes
//! a box at that date play.
bool leads_to(const Scenario::ProcessModel& scenario, TimeVal date)
{
  for(auto& itv : scenario.intervals)
  {
    const auto& start = scenario.state(itv.startState());
    if(start.eventId() == scenario.startEvent().id()
       && std::abs((itv.date() + itv.duration.defaultDuration()).msec() - date.msec()) < 1.)
      return true;
  }
  return false;
}

const Scenario::IntervalModel*
interval_at(const Scenario::ProcessModel& scenario, TimeVal date)
{
  for(auto& itv : scenario.intervals)
    if(std::abs(itv.date().msec() - date.msec()) < 1.)
      return &itv;
  return nullptr;
}
}

TEST_CASE("An audio take sent to the timeline is a looping sound at its date",
          "[looper][timeline][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);
    auto* looper = add_looper(*doc, *scenario, audio_looper_uuid);
    REQUIRE(looper);

    // Two bars at 97 bpm.
    QTemporaryDir dir;
    const QString file = dir.path() + "/Looper-take1.wav";
    const int rate = 48000;
    const int64_t frames = int64_t(std::round(8. * 60. / 97. * rate));
    std::vector<float> l(frames), r(frames);
    for(int64_t i = 0; i < frames; i++)
      l[i] = r[i] = 0.25f * std::sin(i * 0.01f);
    const float* data[2]{l.data(), r.data()};
    ossia::wav_loop_info info;
    info.tempo = 97.;
    info.beats = 8;
    REQUIRE(ossia::write_loop_wav(file.toStdString(), 2, rate, frames, data, info));

    const auto before = scenario->intervals.size();
    oscr::sendToTimeline(
        *looper, doc->context(),
        halp::timeline_clip{.file = file.toStdString(), .date = 1.5, .lane = 0});
    QApplication::processEvents();

    // The box, and the interval from the start that leads to it.
    REQUIRE(scenario->intervals.size() == before + 2);
    CHECK(leads_to(*scenario, TimeVal::fromMsecs(3500)));
    auto* clip = interval_at(*scenario, TimeVal::fromMsecs(3500));
    REQUIRE(clip);
    REQUIRE(clip->processes.size() == 1);
    auto* sound = qobject_cast<const Media::Sound::ProcessModel*>(&*clip->processes.begin());
    REQUIRE(sound);
    CHECK(QFileInfo{sound->file()->absoluteFileName()} == QFileInfo{file});
    // The box holds the take: 2 bars, 8 quarters, 4 s of score's time.
    CHECK(clip->duration.defaultDuration().msec() == Approx(4000.).margin(20.));

    doc->commandStack().undo();
    QApplication::processEvents();
    CHECK(scenario->intervals.size() == before);
  });
}

TEST_CASE("A MIDI take sent to the timeline is a MIDI clip as long as the loop",
          "[looper][timeline][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);
    auto* looper = add_looper(*doc, *scenario, midi_looper_uuid);
    REQUIRE(looper);

    // Four bars at 90 bpm, a note on each bar.
    Nodes::Looper::midi_export e;
    e.rate = 48000.;
    e.tempo = 90.;
    const double fpq = 48000. * 60. / 90.;
    e.length = int64_t(16 * fpq);
    for(int i = 0; i < 4; i++)
      e.notes[i] = {
          .start = int64_t(i * 4 * fpq),
          .length = int64_t(fpq),
          .channel = 1,
          .pitch = uint8_t(60 + i),
          .velocity = 100,
          .pass = 0,
          .replaced_by = -1};
    e.note_count = 4;

    QTemporaryDir dir;
    const QString file = dir.path() + "/MIDI looper-take1.mid";
    {
      const auto bytes = e.to_smf();
      std::ofstream f{file.toStdString(), std::ios::binary};
      f.write(bytes.data(), std::streamsize(bytes.size()));
    }

    const auto before = scenario->intervals.size();
    oscr::sendToTimeline(
        *looper, doc->context(),
        halp::timeline_clip{.file = file.toStdString(), .date = 1., .lane = 0});
    QApplication::processEvents();

    REQUIRE(scenario->intervals.size() == before + 2);
    CHECK(leads_to(*scenario, TimeVal::fromMsecs(3000)));
    auto* clip = interval_at(*scenario, TimeVal::fromMsecs(3000));
    REQUIRE(clip);
    const Midi::ProcessModel* midi{};
    for(auto& p : clip->processes)
      if(auto m = qobject_cast<const Midi::ProcessModel*>(&p))
        midi = m;
    REQUIRE(midi);
    CHECK(midi->notes.size() == 4);
    // 16 quarters at score's 120: 8 s.
    CHECK(clip->duration.defaultDuration().msec() == Approx(8000.).margin(20.));
  });
}
