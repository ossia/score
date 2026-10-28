// A looper's take sent to the timeline becomes a box of the scenario that
// holds the looper, at the date the take was recorded, in one undoable step.

#include <Process/Process.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Drop/ClipMetadata.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/DropProcessInInterval.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/write_loop_wav.hpp>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QUrl>
#include <QTemporaryDir>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/HostServices.hpp>
#include <Crousti/Painter.hpp>
#include <libremidi/detail/conversion.hpp>
#include <score_test/Execution.hpp>
#include <Midi/MidiProcess.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Fx/Looper/LooperView.hpp>
#include <Fx/Looper/MidiLooper.hpp>
#include <Fx/Looper/MidiTake.hpp>
#include <Fx/Looper/ValueTake.hpp>
#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <libremidi/reader.hpp>

#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
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

//! A box from 2 s to `end` of the main scenario, with a looper in it.
Process::ProcessModel* add_looper(
    score::Document& doc, Scenario::ProcessModel& scenario, const QString& uuid,
    TimeVal end = TimeVal::fromMsecs(10000))
{
  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  auto& box = m.createBox(scenario, TimeVal::fromMsecs(2000), end, 0.2);
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
        halp::timeline_clip{.files = {file.toStdString()}, .date = 1.5});
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

namespace
{
//! Two bars at 97 bpm, as the audio looper writes a take: 4.948 s, said to
//! be 8 beats at 97 bpm.
QString write_take_97(const QTemporaryDir& dir, const QString& name = "Looper-take1.wav")
{
  const QString file = dir.path() + "/" + name;
  const int rate = 48000;
  const int64_t frames = int64_t(std::round(8. * 60. / 97. * rate));
  std::vector<float> l(frames), r(frames);
  for(int64_t i = 0; i < frames; i++)
    l[i] = r[i] = 0.25f * std::sin(i * 0.01f);
  const float* data[2]{l.data(), r.data()};
  ossia::wav_loop_info info;
  info.tempo = 97.;
  info.beats = 8;
  if(!ossia::write_loop_wav(file.toStdString(), 2, rate, frames, data, info))
    return {};
  return file;
}

const double take_97_real_msecs = 1000. * std::round(8. * 60. / 97. * 48000) / 48000.;

std::vector<const Media::Sound::ProcessModel*> sounds_in(const Scenario::ProcessModel& scenario)
{
  std::vector<const Media::Sound::ProcessModel*> res;
  for(auto& itv : scenario.intervals)
    for(auto& p : itv.processes)
      if(auto s = qobject_cast<const Media::Sound::ProcessModel*>(&p))
        res.push_back(s);
  return res;
}
}

TEST_CASE("An audio take sent to the timeline plays with the Follow tempo it was made with",
          "[looper][timeline][app][sound]")
{
  using mode = ossia::audio_stretch_mode;
  const std::pair<halp::clip_stretch, mode> cases[]{
      // Without a word from the take, the name says nothing melodic: rhythm.
      {halp::clip_stretch::automatic, mode::RubberBandPercussive},
      {halp::clip_stretch::timestretch, mode::RubberBandStandard},
      {halp::clip_stretch::timestretch_percussive, mode::RubberBandPercussive},
      {halp::clip_stretch::repitch, mode::Repitch},
      {halp::clip_stretch::repitch_fast, mode::RepitchFastestQ},
      {halp::clip_stretch::none, mode::None}};
  for(const auto& [stretch, expected] : cases)
  {
    DYNAMIC_SECTION("stretch " << int(stretch))
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto* scenario = main_scenario(*doc);
        REQUIRE(scenario);
        auto* looper = add_looper(*doc, *scenario, audio_looper_uuid);
        REQUIRE(looper);

        QTemporaryDir dir;
        const QString file = write_take_97(dir);
        REQUIRE(!file.isEmpty());

        const auto before = scenario->intervals.size();
        oscr::sendToTimeline(
            *looper, doc->context(),
            halp::timeline_clip{
                .files = {file.toStdString()}, .date = 1.5, .stretch = stretch});
        QApplication::processEvents();

        REQUIRE(scenario->intervals.size() == before + 2);
        auto* clip = interval_at(*scenario, TimeVal::fromMsecs(3500));
        REQUIRE(clip);
        REQUIRE(clip->processes.size() == 1);
        auto* sound
            = qobject_cast<const Media::Sound::ProcessModel*>(&*clip->processes.begin());
        REQUIRE(sound);
        CHECK(sound->stretchMode() == expected);
        CHECK(sound->loops());
        if(expected == mode::None)
        {
          // Unstretched, it lasts as long as it was recorded: score's time
          // is the clock's at the tempo of a score without a tempo curve.
          CHECK(clip->duration.defaultDuration().msec()
                == Approx(take_97_real_msecs).margin(1.));
          CHECK(sound->loopDuration().msec() == Approx(take_97_real_msecs).margin(1.));
        }
        else
        {
          // Its 8 beats at the score's 120 bpm.
          CHECK(clip->duration.defaultDuration().msec() == Approx(4000.).margin(20.));
          CHECK(sound->loopDuration().msec() == Approx(4000.).margin(20.));
        }

        // One undoable step for the box, the file and its mode.
        doc->commandStack().undo();
        QApplication::processEvents();
        CHECK(scenario->intervals.size() == before);
        CHECK(sounds_in(*scenario).empty());

        doc->commandStack().redo();
        QApplication::processEvents();
        auto again = sounds_in(*scenario);
        REQUIRE(again.size() == 1);
        CHECK(again[0]->stretchMode() == expected);
      });
    }
  }
}

TEST_CASE("A dropped take plays with the stretch its drag carries, and a plain drop guesses",
          "[looper][timeline][app][sound]")
{
  using mode = ossia::audio_stretch_mode;
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);

    QTemporaryDir dir;
    const QString file = write_take_97(dir);
    REQUIRE(!file.isEmpty());

    // What the looper's view drags: the file, and how it is to be played.
    auto drag = [&](halp::clip_stretch stretch) {
      Nodes::Looper::view_message m{};
      m.take = 1;
      const auto base = QString(dir.path() + "/Looper-take1").toStdString();
      m.set_take(base, "wav", true, 0, stretch);
      Nodes::Looper::looper_view view;
      view.receive(m);
      const auto payload = view.drag_payload(view.width() - 20., 10.);
      auto mime = std::make_unique<QMimeData>();
      QList<QUrl> urls;
      for(const auto& f : payload.files)
        urls.push_back(QUrl::fromLocalFile(QString::fromStdString(f)));
      mime->setUrls(urls);
      if(!payload.mime.empty())
        mime->setData(
            QString::fromStdString(payload.mime),
            QByteArray(payload.data.data(), qsizetype(payload.data.size())));
      return mime;
    };

    // Dropped in the scenario as DropProcessInScenario does: a box as long as
    // the drop says, the process made in it.
    auto drop = [&](const QMimeData& mime) -> const Media::Sound::ProcessModel* {
      const auto& handlers = ctx.interfaces<Process::ProcessDropHandlerList>();
      auto res = handlers.getDrop(mime, doc->context());
      if(res.size() != 1)
        return nullptr;
      const auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& box = m.createBox(*scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(1000) + t, 0.5);
      score::Dispatcher_T disp{m};
      auto* p = m.createProcessInNewSlot(box, res.front().creation);
      if(p && res.front().setup)
        res.front().setup(*p, disp);
      m.commit();
      QApplication::processEvents();
      return qobject_cast<const Media::Sound::ProcessModel*>(p);
    };
    auto box_of = [](const Media::Sound::ProcessModel& s) {
      return qobject_cast<const Scenario::IntervalModel*>(s.parent());
    };

    {
      auto mime = drag(halp::clip_stretch::timestretch);
      REQUIRE(mime->hasFormat(QString::fromUtf8(Process::clip_metadata_mime.data())));
      auto* s = drop(*mime);
      REQUIRE(s);
      CHECK(s->stretchMode() == mode::RubberBandStandard);
      CHECK(box_of(*s)->duration.defaultDuration().msec() == Approx(4000.).margin(20.));
      doc->commandStack().undo();
      QApplication::processEvents();
      CHECK(sounds_in(*scenario).empty());
    }
    {
      auto* s = drop(*drag(halp::clip_stretch::none));
      REQUIRE(s);
      CHECK(s->stretchMode() == mode::None);
      CHECK(box_of(*s)->duration.defaultDuration().msec()
            == Approx(take_97_real_msecs).margin(1.));
      CHECK(s->loopDuration().msec() == Approx(take_97_real_msecs).margin(1.));
      doc->commandStack().undo();
      QApplication::processEvents();
      CHECK(sounds_in(*scenario).empty());
    }
    {
      // A take with no stretch of its own: it carries only that it is a
      // loop, and the stretch is guessed from the name.
      auto mime = drag(halp::clip_stretch::automatic);
      const auto data = mime->data(QString::fromUtf8(Process::clip_metadata_mime.data()));
      const auto meta
          = Process::decode_clip_metadata({data.constData(), std::size_t(data.size())});
      REQUIRE(meta.size() == 1);
      CHECK(meta[0].stretch == halp::clip_stretch::automatic);
      CHECK(meta[0].loop);
      auto* s = drop(*mime);
      REQUIRE(s);
      CHECK(s->stretchMode() == mode::RubberBandPercussive);
      CHECK(box_of(*s)->duration.defaultDuration().msec() == Approx(4000.).margin(20.));
    }
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
          .replaced_by = 0};
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
        halp::timeline_clip{.files = {file.toStdString()}, .date = 1.});
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

TEST_CASE("A value take sent to the timeline is an automation for each number, held in steps",
          "[looper][timeline][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);
    auto* looper = add_looper(*doc, *scenario, midi_looper_uuid);
    REQUIRE(looper);

    // Two seconds of vectors: (0, 10) from the start, (1, 20) from a
    // quarter, (0.5, 15) from half of it.
    auto e = std::make_unique<Nodes::Looper::value_export>();
    e->rate = 48000.;
    e->length = 96000;
    Nodes::Looper::value_slot v;
    v.set(ossia::vec2f{0.f, 10.f});
    e->add(0, v, false);
    v.set(ossia::vec2f{1.f, 20.f});
    e->add(24000, v, false);
    v.set(ossia::vec2f{0.5f, 15.f});
    e->add(48000, v, false);

    QTemporaryDir dir;
    auto write = [&](const QString& file, const Nodes::Looper::value_export& e) {
      const auto bytes = e.to_json();
      std::ofstream f{file.toStdString(), std::ios::binary};
      f.write(bytes.data(), std::streamsize(bytes.size()));
    };
    const QString file = dir.path() + "/Value looper-take1.values";
    write(file, *e);

    const auto before = scenario->intervals.size();
    oscr::sendToTimeline(
        *looper, doc->context(),
        halp::timeline_clip{.files = {file.toStdString()}, .date = 1.});
    QApplication::processEvents();

    REQUIRE(scenario->intervals.size() == before + 2);
    auto* clip = interval_at(*scenario, TimeVal::fromMsecs(3000));
    REQUIRE(clip);
    CHECK(clip->duration.defaultDuration().msec() == Approx(2000.).margin(20.));
    std::vector<const Automation::ProcessModel*> autom;
    for(auto& p : clip->processes)
      if(auto a = qobject_cast<const Automation::ProcessModel*>(&p))
        autom.push_back(a);
    REQUIRE(autom.size() == 2);
    std::sort(autom.begin(), autom.end(), [](auto* a, auto* b) {
      return a->metadata().getName() < b->metadata().getName();
    });
    CHECK(autom[0]->min() == 0.);
    CHECK(autom[0]->max() == 1.);
    CHECK(autom[1]->min() == 10.);
    CHECK(autom[1]->max() == 20.);
    for(auto* a : autom)
    {
      // Held between the values, and stepping where they change.
      const auto& c = a->curve();
      CHECK(*c.valueAt(0.1) == Approx(0.));
      CHECK(*c.valueAt(0.24) == Approx(0.));
      CHECK(*c.valueAt(0.26) == Approx(1.));
      CHECK(*c.valueAt(0.49) == Approx(1.));
      CHECK(*c.valueAt(0.51) == Approx(0.5));
      CHECK(*c.valueAt(0.99) == Approx(0.5));
      CHECK(a->loops());
    }

    // Text is a file only: nothing is placed.
    auto text = std::make_unique<Nodes::Looper::value_export>();
    text->rate = 48000.;
    text->length = 96000;
    v.set(std::string{"hello"});
    text->add(0, v, false);
    const QString text_file = dir.path() + "/Value looper-take2.values";
    write(text_file, *text);
    const auto after = scenario->intervals.size();
    oscr::sendToTimeline(
        *looper, doc->context(),
        halp::timeline_clip{.files = {text_file.toStdString()}, .date = 1.});
    QApplication::processEvents();
    CHECK(scenario->intervals.size() == after);
  });
}

TEST_CASE("A value take of lists of numbers is an automation for each index",
          "[looper][timeline][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);
    auto* looper = add_looper(*doc, *scenario, midi_looper_uuid);
    REQUIRE(looper);
    QTemporaryDir dir;
    int files = 0;

    using list = std::vector<ossia::value>;
    // Sends a take of `values`, at 0, a quarter and half of two seconds; the
    // automations it makes, by name.
    auto send = [&](std::vector<ossia::value> values) {
      Nodes::Looper::value_export e;
      e.rate = 48000.;
      e.length = 96000;
      Nodes::Looper::value_slot v;
      for(std::size_t i = 0; i < values.size(); i++)
      {
        v.set(values[i]);
        e.add(int64_t(i) * 24000, v, false);
      }
      const QString file
          = dir.path() + "/Arraygen-take" + QString::number(++files) + ".values";
      const auto bytes = e.to_json();
      std::ofstream f{file.toStdString(), std::ios::binary};
      f.write(bytes.data(), std::streamsize(bytes.size()));
      f.close();
      std::set<const Scenario::IntervalModel*> before;
      for(auto& itv : scenario->intervals)
        before.insert(&itv);
      oscr::sendToTimeline(
          *looper, doc->context(),
          halp::timeline_clip{.files = {file.toStdString()}, .date = 10. * files});
      QApplication::processEvents();
      std::vector<const Automation::ProcessModel*> autom;
      const Scenario::IntervalModel* clip{};
      for(auto& itv : scenario->intervals)
        if(!before.contains(&itv) && !itv.processes.empty())
          clip = &itv;
      if(!clip)
        return autom;
      for(auto& p : clip->processes)
        if(auto a = qobject_cast<const Automation::ProcessModel*>(&p))
          autom.push_back(a);
      std::sort(autom.begin(), autom.end(), [](auto* a, auto* b) {
        return a->metadata().getName() < b->metadata().getName();
      });
      return autom;
    };

    // Lists of four floats, as Arraygen sends: one automation for each.
    {
      const auto autom = send(
          {list{0.f, 10.f, 20.f, 30.f}, list{1.f, 11.f, 22.f, 33.f},
           list{0.5f, 10.5f, 21.f, 31.5f}});
      REQUIRE(autom.size() == 4);
      CHECK(autom[0]->metadata().getName().endsWith(" 0"));
      CHECK(autom[3]->metadata().getName().endsWith(" 3"));
      const double lo[]{0., 10., 20., 30.}, hi[]{1., 11., 22., 33.};
      for(int c = 0; c < 4; c++)
      {
        INFO(c);
        CHECK(autom[c]->min() == Approx(lo[c]));
        CHECK(autom[c]->max() == Approx(hi[c]));
        const auto& curve = autom[c]->curve();
        CHECK(*curve.valueAt(0.1) == Approx(0.));
        CHECK(*curve.valueAt(0.26) == Approx(1.));
        CHECK(*curve.valueAt(0.51) == Approx(0.5));
      }
    }

    // Lists of varying length: an index a list does not have holds where it
    // was, and before it first comes, its first value holds. A number counts
    // as a list of one.
    {
      const auto autom = send({list{1, 2.f}, 3.f, list{5.f, 6.f, true}});
      REQUIRE(autom.size() == 3);
      CHECK(autom[0]->min() == Approx(1.));
      CHECK(autom[0]->max() == Approx(5.));
      CHECK(*autom[0]->curve().valueAt(0.26) == Approx(0.5));
      CHECK(autom[1]->min() == Approx(2.));
      CHECK(autom[1]->max() == Approx(6.));
      CHECK(*autom[1]->curve().valueAt(0.1) == Approx(0.));
      CHECK(*autom[1]->curve().valueAt(0.26) == Approx(0.));
      CHECK(*autom[1]->curve().valueAt(0.51) == Approx(1.));
      // Only the last list has a third number: it holds all along.
      CHECK(autom[2]->min() == Approx(1.));
      CHECK(*autom[2]->curve().valueAt(0.1) == Approx(0.));
      CHECK(*autom[2]->curve().valueAt(0.9) == Approx(0.));
    }

    // Nested lists, text in a list, more than 32 numbers: a file only.
    CHECK(send({list{list{1.f, 2.f}, 3.f}}).empty());
    CHECK(send({list{1.f, std::string{"a"}}}).empty());
    CHECK(send({list(33, ossia::value{1.f})}).empty());
    CHECK(send({list(32, ossia::value{1.f}), list(32, ossia::value{2.f})}).size() == 32);
  });
}

TEST_CASE("A take that began before the scenario is placed at its start, whole",
          "[looper][timeline][app]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);
    auto* looper = add_looper(*doc, *scenario, audio_looper_uuid);
    REQUIRE(looper);

    // Two bars at 120 bpm: 4 s.
    QTemporaryDir dir;
    const QString file = dir.path() + "/Looper-take1.wav";
    const int rate = 48000;
    const int64_t frames = 4 * rate;
    std::vector<float> l(frames), r(frames);
    for(int64_t i = 0; i < frames; i++)
      l[i] = r[i] = 0.25f * std::sin(i * 0.01f);
    const float* data[2]{l.data(), r.data()};
    ossia::wav_loop_info info;
    info.tempo = 120.;
    info.beats = 8;
    REQUIRE(ossia::write_loop_wav(file.toStdString(), 2, rate, frames, data, info));

    // Captured 3 s before the looper's box, which starts at 2 s: a second
    // before the scenario's start.
    const auto before = scenario->intervals.size();
    oscr::sendToTimeline(
        *looper, doc->context(),
        halp::timeline_clip{.files = {file.toStdString()}, .date = -3.});
    QApplication::processEvents();

    // The box, straight off the scenario's start, with nothing leading to it.
    REQUIRE(scenario->intervals.size() == before + 1);
    const Scenario::IntervalModel* clip{};
    for(auto& itv : scenario->intervals)
      for(auto& p : itv.processes)
        if(qobject_cast<const Media::Sound::ProcessModel*>(&p))
          clip = &itv;
    REQUIRE(clip);
    CHECK(clip->date().msec() == Approx(0.).margin(1.));
    CHECK(clip->duration.defaultDuration().msec() == Approx(4000.).margin(20.));
  });
}

namespace
{
using MidiLooper = Nodes::MidiLooper::Node;

//! The MIDI looper driven as the ossia binding drives it, with the take's
//! files written at once and its view messages kept, at `tempo` in 4/4.
struct midi_looper_run
{
  MidiLooper node;
  ossia::execution_state st;
  int buffer_size{500};
  int64_t now{0};
  double tempo{90.};
  //! Quarters in a bar.
  int meter{4};
  std::vector<libremidi::message> pending;
  std::unique_ptr<Nodes::Looper::view_message> last = std::make_unique<Nodes::Looper::view_message>();
  Nodes::Looper::looper_view view;

  midi_looper_run(const QString& folder, Nodes::Looper::TakeFiles files)
  {
    st.sampleRate = 48000;
    st.bufferSize = buffer_size;
    st.modelToSamplesRatio = 1.;
    st.samplesToModelRatio = 1.;
    node.ossia_state = {&st};
    node.inputs.quantif.value = halp::grid_value{};
    node.inputs.late.value = 0;
    node.inputs.grace.value = 0;
    node.inputs.postaction_bars.value = 0;
    node.inputs.passthrough = MidiLooper::Passthrough::None;
    node.inputs.start_from = Nodes::Looper::StartFrom::WhereItWas;
    node.inputs.exported.value = files;
    node.inputs.midi.midi_messages.reserve(64);
    node.outputs.midi.midi_messages.reserve(1024);

    auto writer = std::make_shared<Nodes::Looper::take_writer>();
    writer->folder = folder;
    writer->prefix = "MIDI looper";
    writer->extension = "mid";
    node.writer = writer;
    node.worker.request = [this](Nodes::Looper::midi_take_job job) {
      MidiLooper::worker_t::work(std::move(job))(node);
    };
    node.send_message = [this](const Nodes::Looper::view_message& m) {
      *last = m;
      view.receive(m);
    };
  }

  void note(int pitch, int at, int len)
  {
    auto on = libremidi::channel_events::note_on(1, pitch, 100);
    on.timestamp = at;
    pending.push_back(on);
    auto off = libremidi::channel_events::note_off(1, pitch, 0);
    off.timestamp = at + len;
    pending.push_back(off);
  }

  void run()
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{now};
    tk.date = ossia::time_value{now + buffer_size};
    tk.start_sample = 0;
    tk.length_sample = buffer_size;
    tk.tempo = tempo;
    const double qps = tempo / 60. / st.sampleRate;
    tk.musical_start_position = now * qps;
    tk.musical_end_position = (now + buffer_size) * qps;
    tk.signature = {uint16_t(meter), 4};
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / meter) * meter;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / meter) * meter;
    st.samples_since_start = now + buffer_size;

    auto& in = node.inputs.midi.midi_messages;
    in.clear();
    for(auto& m : pending)
      in.push_back(m);
    pending.clear();
    node.outputs.midi.midi_messages.clear();
    node(tk);
    now += buffer_size;
  }
  void run_frames(int64_t frames)
  {
    for(int64_t i = 0; i < frames; i += buffer_size)
      run();
  }

  //! A pass of `frames`, a recording when the loop is empty, with `pitch`
  //! played at its start; then the loop plays for a while.
  void pass(MidiLooper::LoopMode mode, int pitch, int64_t frames)
  {
    node.inputs.mode = mode;
    note(pitch, 10, 400);
    run_frames(frames);
    node.inputs.mode = MidiLooper::LoopMode::Play;
    run_frames(48000);
  }
};

std::set<int> pitches_in(const QString& file)
{
  QFile f{file};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  const auto bytes = f.readAll();
  libremidi::reader r;
  std::vector<uint8_t> raw(bytes.begin(), bytes.end());
  r.parse(raw);
  std::set<int> res;
  for(auto& t : r.tracks)
    for(auto& ev : t)
      if(ev.m.get_message_type() == libremidi::message_type::NOTE_ON && ev.m.bytes[2] > 0)
        res.insert(ev.m.bytes[1]);
  return res;
}

std::set<int> pitches_in(const Midi::ProcessModel& midi)
{
  std::set<int> res;
  for(auto& n : midi.notes)
    res.insert(n.pitch());
  return res;
}

//! The MIDI clips the timeline has, by their box's name.
std::map<QString, const Midi::ProcessModel*> midi_clips(const Scenario::ProcessModel& scenario)
{
  std::map<QString, const Midi::ProcessModel*> res;
  for(auto& itv : scenario.intervals)
    for(auto& p : itv.processes)
      if(auto m = qobject_cast<const Midi::ProcessModel*>(&p))
        res[m->metadata().getName()] = m;
  return res;
}
}

TEST_CASE("The MIDI looper's takes in the timeline: each file holds its pass, each take "
          "the loop as it was then; clips loop at the loop's length",
          "[looper][timeline][app][midi]")
{
  using LoopMode = MidiLooper::LoopMode;
  using Nodes::Looper::TakeFiles;
  for(const auto files : {TakeFiles::Layers, TakeFiles::Both, TakeFiles::Mix})
  {
    DYNAMIC_SECTION("Export " << int(files))
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto* scenario = main_scenario(*doc);
        REQUIRE(scenario);
        auto* looper = add_looper(*doc, *scenario, midi_looper_uuid);
        REQUIRE(looper);

        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        midi_looper_run h{dir.path(), files};
        h.view.send_to_timeline = [&](halp::timeline_clip clip) {
          oscr::sendToTimeline(*looper, doc->context(), clip);
        };

        // Four bars at 90 bpm, then two overdubs of one round each.
        const int64_t four_bars = 16 * 32000;
        h.pass(LoopMode::Record, 60, four_bars);
        REQUIRE(h.node.loop.length == four_bars);
        h.pass(LoopMode::Overdub, 64, four_bars);
        h.pass(LoopMode::Overdub, 67, four_bars);
        REQUIRE(h.node.loop.count == 3);

        const QString base = dir.path() + "/MIDI looper-take";
        auto layer = [&](int take, int k) {
          return base + QString::number(take) + "-layer" + QString::number(k) + ".mid";
        };
        auto mix = [&](int take) { return base + QString::number(take) + ".mid"; };
        if(files != TakeFiles::Mix)
        {
          CHECK(pitches_in(layer(1, 1)) == std::set<int>{60});
          CHECK(pitches_in(layer(2, 1)) == std::set<int>{60});
          CHECK(pitches_in(layer(2, 2)) == std::set<int>{64});
          CHECK(pitches_in(layer(3, 1)) == std::set<int>{60});
          CHECK(pitches_in(layer(3, 2)) == std::set<int>{64});
          CHECK(pitches_in(layer(3, 3)) == std::set<int>{67});
          CHECK(!QFileInfo::exists(layer(3, 4)));
        }
        if(files != TakeFiles::Layers)
        {
          CHECK(pitches_in(mix(1)) == std::set<int>{60});
          CHECK(pitches_in(mix(2)) == std::set<int>{60, 64});
          CHECK(pitches_in(mix(3)) == std::set<int>{60, 64, 67});
        }
        else
        {
          CHECK(!QFileInfo::exists(mix(3)));
        }

        // The last take goes to the timeline with the view's Send: one box, a
        // clip per file.
        h.view.send();
        QApplication::processEvents();
        const auto clips = midi_clips(*scenario);
        const std::size_t expected = (files != TakeFiles::Layers ? 1 : 0)
                                     + (files != TakeFiles::Mix ? 3 : 0);
        REQUIRE(clips.size() == expected);

        // The box is named after the take, its slots in the order of the files:
        // the mix first.
        const auto& box = *qobject_cast<const Scenario::IntervalModel*>(
            clips.begin()->second->parent());
        CHECK(box.metadata().getName() == "MIDI looper-take3");
        CHECK(box.processes.size() == expected);
        REQUIRE(box.smallView().size() == expected);
        {
          std::vector<QString> order;
          for(const auto& slot : box.smallView())
            order.push_back(box.processes.at(*slot.frontProcess).metadata().getName());
          std::vector<QString> files_order;
          if(files != TakeFiles::Layers)
            files_order.push_back("MIDI looper-take3");
          if(files != TakeFiles::Mix)
            for(int k = 1; k <= 3; k++)
              files_order.push_back("MIDI looper-take3-layer" + QString::number(k));
          CHECK(order == files_order);
        }
        // Four bars of score's time: 16 quarters of half a second.
        CHECK(box.duration.defaultDuration().msec() == Approx(8000.).margin(1.));

        auto check_clip = [&](const QString& name, std::set<int> pitches) {
          INFO(name.toStdString());
          auto it = clips.find(name);
          REQUIRE(it != clips.end());
          const auto& midi = *it->second;
          CHECK(pitches_in(midi) == pitches);
          CHECK(midi.parent() == &box);
          CHECK(midi.duration().msec() == Approx(8000.).margin(1.));
          // The clip loops, at the loop's length.
          CHECK(midi.loops());
          CHECK(midi.loopDuration().msec() == Approx(8000.).margin(1.));
        };
        if(files != TakeFiles::Layers)
          check_clip("MIDI looper-take3", {60, 64, 67});
        if(files != TakeFiles::Mix)
        {
          check_clip("MIDI looper-take3-layer1", {60});
          check_clip("MIDI looper-take3-layer2", {64});
          check_clip("MIDI looper-take3-layer3", {67});
        }

        // One undoable step for the whole take.
        doc->commandStack().undo();
        QApplication::processEvents();
        CHECK(midi_clips(*scenario).empty());
      });
    }
  }
}

TEST_CASE("A MIDI take recorded in the engine has the loop's quarters at the score's tempo",
          "[looper][timeline][app][midi][exec]")
{
  for(const bool by_hand : {true, false})
  for(const double tempo : {90., 120., 60.})
  {
    DYNAMIC_SECTION("at " << tempo << " bpm, " << (by_hand ? "stopped by hand" : "four bars of post-action"))
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto* proc = score::test::add_process(*doc, midi_looper_uuid, {});
        REQUIRE(proc);

        auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
        plug.reload(true, score::test::base_interval(*doc));
        score::test::run_exec(plug);
        REQUIRE(plug.baseScenario());
        auto& base = *plug.baseScenario();
        auto& procs = base.baseInterval().processes();
        auto it = procs.find(proc->id());
        REQUIRE(it != procs.end());
        auto& node = static_cast<oscr::safe_node<MidiLooper>&>(*it->second->node);
        auto& looper = node.impl.effect;
        auto& in = *node.root_inputs()[0]->template target<ossia::midi_port>();

        QTemporaryDir dir;
        auto writer = std::make_shared<Nodes::Looper::take_writer>();
        writer->folder = dir.path();
        writer->prefix = "MIDI looper";
        writer->extension = "mid";
        looper.writer = writer;
        looper.worker.request = [&](Nodes::Looper::midi_take_job job) {
          MidiLooper::worker_t::work(std::move(job))(looper);
        };

        auto& ectx = plug.context();
        auto& st = *ectx.execState;
        auto& itv = *base.baseInterval().OSSIAInterval();
        ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
        {
          ossia::tempo_curve curve;
          curve.set_x0(0);
          curve.set_y0(tempo);
          curve.add_point(
              [](double, double a, double b) { return a + 0. * b; },
              int64_t(1e6 * ossia::flicks_per_second<double>), tempo);
          itv.set_tempo_curve(std::move(curve));
        }
        const ossia::time_value end{int64_t(1e5 * ossia::flicks_per_second<double>)};
        itv.set_min_duration(end);
        itv.set_nominal_duration(end);
        itv.set_max_duration(end);
        itv.start_and_tick();
        ossia::buffer_tick tick{st, *ectx.execGraph, base.baseScenario(), {}};

        // Ticks as the engine gives them: never more than the buffer it was set up with.
        const int buffer = st.bufferSize;
        int64_t frames = 0;
        auto run = [&](int64_t n) {
          for(int64_t i = 0; i < n; i += buffer)
          {
            tick(buffer, double(frames) / st.sampleRate);
            frames += buffer;
            in.messages.clear();
          }
        };

        // Four bars at the score's tempo, a note on the first beat.
        const int64_t four_bars = int64_t(std::llround(16. * 60. / tempo * st.sampleRate));
        if(by_hand)
        {
          looper.inputs.quantif.value = halp::grid_value{};
          looper.inputs.postaction_bars.value = 0;
        }
        else
        {
          // The defaults: the recording starts on the next bar and gives way
          // to the post-action after four bars, while Loop stays on Record.
          REQUIRE(looper.inputs.postaction_bars.value == 4);
          run(int64_t(std::llround(2. * 60. / tempo * st.sampleRate)));
        }
        looper.inputs.mode = MidiLooper::LoopMode::Record;
        if(!by_hand)
          run(int64_t(std::llround(2. * 60. / tempo * st.sampleRate)));
        in.messages.push_back(
            libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 60, 100)));
        run(buffer);
        in.messages.push_back(
            libremidi::ump_from_midi1(libremidi::channel_events::note_off(1, 60, 0)));
        run(four_bars - buffer);
        if(by_hand)
          looper.inputs.mode = MidiLooper::LoopMode::Play;
        else
          run(four_bars);
        run(48000);
        ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

        INFO("rate " << st.sampleRate << ", loop " << looper.loop.length << " frames");
        CHECK(std::abs(double(looper.loop.length - four_bars)) <= buffer);

        // The file holds 16 quarters, read by score as 16 half-seconds.
        const QString file = dir.path() + "/MIDI looper-take1.mid";
        REQUIRE(QFileInfo::exists(file));
        QFile f{file};
        REQUIRE(f.open(QIODevice::ReadOnly));
        const auto bytes = f.readAll();
        libremidi::reader r;
        std::vector<uint8_t> raw(bytes.begin(), bytes.end());
        r.parse(raw);
        // A stop by hand takes effect at a buffer's end: as far off as the loop.
        const double buffer_quarters = double(buffer) / st.sampleRate * tempo / 60.;
        CHECK(r.get_end_time() / r.ticksPerBeat == Approx(16.).margin(0.05 + buffer_quarters));

        plug.clear();
        QApplication::processEvents();
      });
    }
  }
}

TEST_CASE("A MIDI take's clip is one process, and its loop settings survive a reload",
          "[looper][timeline][app][midi]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* scenario = main_scenario(*doc);
    REQUIRE(scenario);
    auto* looper = add_looper(*doc, *scenario, midi_looper_uuid);
    REQUIRE(looper);

    // Eight bars at 120 bpm, a note on each of the first four.
    Nodes::Looper::midi_export e;
    e.rate = 48000.;
    e.tempo = 120.;
    const double fpq = 24000.;
    e.length = int64_t(32 * fpq);
    for(int i = 0; i < 4; i++)
      e.notes[i] = {
          .start = int64_t(i * 4 * fpq),
          .length = int64_t(fpq),
          .channel = 1,
          .pitch = uint8_t(60 + i),
          .velocity = 100,
          .pass = 0,
          .replaced_by = 0};
    e.note_count = 4;
    QTemporaryDir dir;
    const QString file = dir.path() + "/MIDI looper-take1.mid";
    {
      const auto bytes = e.to_smf();
      std::ofstream f{file.toStdString(), std::ios::binary};
      f.write(bytes.data(), std::streamsize(bytes.size()));
    }
    oscr::sendToTimeline(
        *looper, doc->context(),
        halp::timeline_clip{.files = {file.toStdString()}, .date = 1.});
    QApplication::processEvents();

    auto clips_of = [](score::Document& d) {
      std::vector<Midi::ProcessModel*> res;
      for(auto* m : d.model().modelDelegate().findChildren<Midi::ProcessModel*>())
        res.push_back(m);
      return res;
    };
    auto clips = clips_of(*doc);
    // A file with one track is one MIDI process, whatever the case of the
    // extensions its handler lists.
    REQUIRE(clips.size() == 1);
    CHECK(clips[0]->loops());
    CHECK(clips[0]->loopDuration().msec() == Approx(16000.).margin(1.));

    // Looped at four bars by hand, as the inspector does.
    doc->commandStack().redoAndPush(new Process::SetLoopDuration{*clips[0], TimeVal::fromMsecs(8000)});

    auto* reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    auto again = clips_of(*reloaded);
    REQUIRE(again.size() == 1);
    CHECK(again[0]->loops());
    CHECK(again[0]->loopDuration().msec() == Approx(8000.).margin(1.));
    CHECK(again[0]->duration().msec() == Approx(16000.).margin(1.));
    CHECK(again[0]->notes.size() == 4);
  });
}

namespace
{
//! A MIDI looper in a box of the main scenario, run by the engine at `tempo`
//! in `meter`/4 with the looper's defaults, as in the app: its view gets the
//! looper's messages, and asks it for the files of a lane.
struct engine_midi_looper
{
  score::Document& doc;
  Scenario::ProcessModel& scenario;
  Execution::DocumentPlugin& plug;
  double tempo{};
  int meter{};
  TimeVal box_start{};
  Process::ProcessModel* proc{};
  Scenario::IntervalModel* box{};
  MidiLooper* looper{};
  ossia::midi_port* in{};
  std::optional<ossia::buffer_tick> ticker;
  int64_t frames{};
  //! Where the recording started in the timeline.
  TimeVal record_date{};
  Nodes::Looper::looper_view view;
  QTemporaryDir dir;

  static TimeVal box_duration() { return TimeVal::fromMsecs(600000); }

  engine_midi_looper(
      score::Document& d, Scenario::ProcessModel& s, double tempo, int meter,
      TimeVal box_start)
      : doc{d}
      , scenario{s}
      , plug{d.context().plugin<Execution::DocumentPlugin>()}
      , tempo{tempo}
      , meter{meter}
      , box_start{box_start}
  {
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
      auto& from = m.createState(scenario, scenario.startEvent().id(), 0.2);
      Id<Scenario::StateModel> start = from.id();
      if(box_start > TimeVal::zero())
        start = m.createIntervalAfter(scenario, from.id(), {box_start, 0.2}).endState();
      box = &m.createIntervalAfter(scenario, start, {box_start + box_duration(), 0.2});
      proc = m.createProcessInNewSlot(
          *box, UuidKey<Process::ProcessModel>::fromString(midi_looper_uuid), QString{});
      m.commit();
    }
    REQUIRE(proc);

    plug.reload(true, score::test::base_interval(doc));
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    Execution::IntervalComponent* comp{};
    for(auto* c : box->components())
      if(auto ic = dynamic_cast<Execution::IntervalComponent*>(c))
        comp = ic;
    REQUIRE(comp);
    auto it = comp->processes().find(proc->id());
    REQUIRE(it != comp->processes().end());
    auto& node = static_cast<oscr::safe_node<MidiLooper>&>(*it->second->node);
    looper = &node.impl.effect;
    in = node.root_inputs()[0]->template target<ossia::midi_port>();
    REQUIRE(in);

    auto writer = std::make_shared<Nodes::Looper::take_writer>();
    writer->folder = dir.path();
    writer->prefix = "MIDI looper";
    writer->extension = "mid";
    looper->writer = writer;
    looper->worker.request = [this](Nodes::Looper::midi_take_job job) {
      MidiLooper::worker_t::work(std::move(job))(*looper);
    };
    looper->send_message
        = [this](const Nodes::Looper::view_message& m) { view.receive(m); };
    view.erase = [this](Nodes::Looper::erase_request r) { looper->process_message(r); };
    view.send_to_timeline = [this](halp::timeline_clip clip) {
      oscr::sendToTimeline(*proc, doc.context(), clip);
    };

    auto& itv = *plug.baseScenario()->baseInterval().OSSIAInterval();
    {
      ossia::tempo_curve curve;
      curve.set_x0(0);
      curve.set_y0(tempo);
      curve.add_point(
          [](double, double a, double b) { return a + 0. * b; },
          int64_t(1e6 * ossia::flicks_per_second<double>), tempo);
      itv.set_tempo_curve(std::move(curve));
    }
    itv.set_time_signature_map(ossia::time_signature_map{
        {ossia::time_value{}, ossia::time_signature{uint16_t(meter), 4}}});
    const ossia::time_value end{int64_t(1e5 * ossia::flicks_per_second<double>)};
    itv.set_min_duration(end);
    itv.set_nominal_duration(end);
    itv.set_max_duration(end);
    itv.start_and_tick();
    auto& ectx = plug.context();
    ticker.emplace(ossia::buffer_tick{
        *ectx.execState, *ectx.execGraph, plug.baseScenario()->baseScenario(), {}});
  }

  double rate() const { return plug.context().execState->sampleRate; }
  int64_t quarter() const { return std::llround(60. / tempo * rate()); }
  int64_t bar() const { return meter * quarter(); }
  //! Four bars of timeline time: a quarter is half a second of it.
  TimeVal four_bars() const { return TimeVal::fromMsecs(4. * meter * 500.); }

  //! Runs `n` frames, the messages at their frame from now.
  void run(int64_t n, std::vector<std::pair<int64_t, libremidi::message>> msgs = {})
  {
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    const int buffer = plug.context().execState->bufferSize;
    for(int64_t i = 0; i < n; i += buffer)
    {
      for(auto& [at, m] : msgs)
        if(at >= i && at < i + buffer)
        {
          auto u = libremidi::ump_from_midi1(m);
          u.timestamp = at - i;
          in->messages.push_back(u);
        }
      (*ticker)(buffer, double(frames) / rate());
      frames += buffer;
      in->messages.clear();
    }
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    QApplication::processEvents();
  }

  //! Four bars recorded with the defaults, from a bar of the timeline: Record
  //! pressed just before it starts there, on the grid, and the post-action
  //! ends it four bars on. A short note on the first beat of each bar,
  //! pitch 60 + bar.
  void record_four_bars()
  {
    run(int64_t(box_start.msec() / 1000. * 120. / tempo * rate()) + bar() / 2);
    int64_t bar_at = (frames / bar() + 1) * bar();
    if(bar_at - quarter() / 2 <= frames)
      bar_at += bar();
    run(bar_at - quarter() / 2 - frames);
    looper->inputs.mode = MidiLooper::LoopMode::Record;
    std::vector<std::pair<int64_t, libremidi::message>> notes;
    const int64_t to_bar = bar_at - frames;
    for(int b = 0; b < 4; b++)
    {
      notes.push_back(
          {to_bar + b * bar() + 100, libremidi::channel_events::note_on(1, 60 + b, 100)});
      notes.push_back(
          {to_bar + b * bar() + quarter() / 2,
           libremidi::channel_events::note_off(1, 60 + b, 0)});
    }
    run(to_bar + 5 * bar(), notes);
    record_date = TimeVal::fromMsecs(double(bar_at / bar()) * meter * 500.);
  }

  double lane_y(int lane) const
  {
    return view.lanes_top + (lane + 0.5) * view.lane_height();
  }

  //! What a drag of the take from the Send button carries.
  std::unique_ptr<QMimeData> drag_take()
  {
    return std::unique_ptr<QMimeData>{
        oscr::make_drag_mime(view.drag_payload(view.width() - 20., 10.))};
  }

  //! What a drag of the first lane carries, once the looper wrote its file.
  std::unique_ptr<QMimeData> drag_lane()
  {
    REQUIRE(view.mouse_press(100., lane_y(0)));
    for(int i = 0; i < 64 && view.preparing(0); i++)
      run(quarter());
    REQUIRE(!view.preparing(0));
    auto payload = view.drag_payload(100., lane_y(0));
    view.mouse_release(100., lane_y(0));
    REQUIRE(payload.files.size() == 1);
    return std::unique_ptr<QMimeData>{oscr::make_drag_mime(payload)};
  }

  //! Dropped in the scenario as DropProcessInScenario does: a box at 1 s as
  //! long as the drop says, a process in it for each of what it holds.
  void drop_in_scenario(const QMimeData& mime)
  {
    const auto& handlers = doc.context().app.interfaces<Process::ProcessDropHandlerList>();
    auto res = handlers.getDrop(mime, doc.context());
    REQUIRE(!res.empty());
    const auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));
    Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
    auto& b = m.createBox(scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(1000) + t, 0.6);
    score::Dispatcher_T disp{m};
    for(auto& r : res)
    {
      auto* p = m.createProcessInNewSlot(b, r.creation);
      if(p && r.setup)
        r.setup(*p, disp);
    }
    m.commit();
    QApplication::processEvents();
  }

  //! Dropped on the box of the looper itself, where a drag from its view
  //! ends when it does not go far.
  void drop_on_looper_box(const QMimeData& mime)
  {
    Scenario::DropProcessInInterval{}.drop(doc.context(), *box, {}, mime);
    QApplication::processEvents();
  }
};

//! How a take is placed in the timeline.
enum class Placing
{
  Send,
  DragTake,
  DragLane,
  DragTakeOnBox,
  DragLaneOnBox
};
const char* name(Placing p)
{
  switch(p)
  {
    case Placing::Send:
      return "Send";
    case Placing::DragTake:
      return "take dragged";
    case Placing::DragLane:
      return "lane dragged";
    case Placing::DragTakeOnBox:
      return "take dragged on the looper's box";
    case Placing::DragLaneOnBox:
      return "lane dragged on the looper's box";
  }
  return "";
}
}

TEST_CASE(
    "A MIDI loop of four bars recorded in a box is a clip looping at four bars, "
    "sent or dragged, whole or a layer",
    "[looper][timeline][app][midi][exec]")
{
  struct setup
  {
    double tempo;
    int meter;
    double box_start;
  };
  for(const auto [tempo, meter, box_start] :
      {setup{120., 4, 0.}, setup{90., 4, 0.}, setup{120., 3, 0.}, setup{120., 4, 7000.},
       setup{90., 3, 7000.}})
  for(const auto placing :
      {Placing::Send, Placing::DragTake, Placing::DragLane, Placing::DragTakeOnBox,
       Placing::DragLaneOnBox})
  {
    DYNAMIC_SECTION(
        tempo << " bpm in " << meter << "/4, looper's box at " << box_start << " ms, "
              << name(placing))
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto* scenario = main_scenario(*doc);
        REQUIRE(scenario);
        engine_midi_looper e{
            *doc, *scenario, tempo, meter, TimeVal::fromMsecs(box_start)};
        // A take of the mix and of the pass: Send places both.
        if(placing == Placing::Send)
          e.looper->inputs.exported.value = Nodes::Looper::TakeFiles::Both;
        e.record_four_bars();

        INFO("loop of " << e.looper->loop.length << " frames, a bar is " << e.bar());
        REQUIRE(std::abs(double(e.looper->loop.length - 4 * e.bar())) <= 1.);
        REQUIRE(e.view.state.take == 1);

        // The take's files hold the four bars' quarters.
        for(const auto& f : e.view.state.take_files())
        {
          INFO(f);
          QFile file{QString::fromStdString(f)};
          REQUIRE(file.open(QIODevice::ReadOnly));
          const auto bytes = file.readAll();
          libremidi::reader r;
          std::vector<uint8_t> raw(bytes.begin(), bytes.end());
          r.parse(raw);
          CHECK(double(r.get_end_time()) / r.ticksPerBeat == Approx(4. * meter).margin(0.01));
        }

        const auto before = midi_clips(*scenario);
        CHECK(before.empty());
        switch(placing)
        {
          case Placing::Send:
            e.view.send();
            break;
          case Placing::DragTake:
            e.drop_in_scenario(*e.drag_take());
            break;
          case Placing::DragLane:
            e.drop_in_scenario(*e.drag_lane());
            break;
          case Placing::DragTakeOnBox:
            e.drop_on_looper_box(*e.drag_take());
            break;
          case Placing::DragLaneOnBox:
            e.drop_on_looper_box(*e.drag_lane());
            break;
        }
        QApplication::processEvents();

        const auto clips = midi_clips(*scenario);
        const bool on_box
            = placing == Placing::DragTakeOnBox || placing == Placing::DragLaneOnBox;
        const bool lane = placing == Placing::DragLane || placing == Placing::DragLaneOnBox;
        std::set<QString> names;
        for(const auto& [n, _] : clips)
          names.insert(n);
        if(placing == Placing::Send)
          CHECK(names == std::set<QString>{"MIDI looper-take1", "MIDI looper-take1-layer1"});
        else if(lane)
          REQUIRE(names.size() == 1);
        else
          CHECK(names == std::set<QString>{"MIDI looper-take1"});
        REQUIRE(!clips.empty());

        const double bars4 = e.four_bars().msec();
        for(const auto& [n, midi] : clips)
        {
          INFO(n.toStdString());
          const auto& clip_box = *qobject_cast<const Scenario::IntervalModel*>(midi->parent());
          if(on_box)
          {
            // In the looper's box, the clip is as long as the box and plays
            // the four bars round.
            CHECK(&clip_box == e.box);
            CHECK(midi->duration() == e.box_duration());
          }
          else
          {
            CHECK(clip_box.duration.defaultDuration().msec() == Approx(bars4).margin(1.));
            CHECK(midi->duration().msec() == Approx(bars4).margin(1.));
          }
          CHECK(midi->loops());
          CHECK(midi->loopDuration().msec() == Approx(bars4).margin(1.));
          if(placing == Placing::Send)
            CHECK(clip_box.date().msec() == Approx(e.record_date.msec()).margin(1.));

          // A note on the first beat of each bar of the loop.
          std::map<int, double> starts;
          for(const auto& note : midi->notes)
            starts[note.pitch()] = note.start() * midi->duration().msec();
          REQUIRE(starts.size() == 4);
          for(int b = 0; b < 4; b++)
          {
            INFO("bar " << b);
            REQUIRE(starts.contains(60 + b));
            CHECK(starts[60 + b] == Approx(b * bars4 / 4.).margin(10.));
          }
        }

        e.plug.clear();
        QApplication::processEvents();
      });
    }
  }
}

TEST_CASE("Audio and value takes loop at their length, sent, dragged, or dropped on a box",
          "[looper][timeline][app][sound]")
{
  enum class where
  {
    send,
    new_box,
    on_box
  };
  for(const auto w : {where::send, where::new_box, where::on_box})
  for(const bool audio : {true, false})
  {
    DYNAMIC_SECTION(
        (audio ? "audio take, " : "value take, ")
        << (w == where::send ? "sent" : w == where::new_box ? "dragged" : "dropped on a box"))
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto* scenario = main_scenario(*doc);
        REQUIRE(scenario);
        // The looper's box, from 2 s to 22 s.
        auto* looper = add_looper(
            *doc, *scenario, audio ? audio_looper_uuid : midi_looper_uuid,
            TimeVal::fromMsecs(22000));
        REQUIRE(looper);
        auto& looper_box = *qobject_cast<Scenario::IntervalModel*>(looper->parent());

        QTemporaryDir dir;
        // Audio: two bars at 97 bpm, 4 s of score's time. Values: four bars at
        // 90 bpm, 10.7 s, 8 s of score's time; 0 then 1 from half of it.
        QString file;
        double length{};
        const auto stretch
            = audio ? halp::clip_stretch::timestretch : halp::clip_stretch::automatic;
        if(audio)
        {
          file = write_take_97(dir);
          length = 4000.;
        }
        else
        {
          auto e = std::make_unique<Nodes::Looper::value_export>();
          e->rate = 48000.;
          e->tempo = 90.;
          e->length = std::llround(16. * 60. / 90. * 48000.);
          Nodes::Looper::value_slot v;
          v.set(0.f);
          e->add(0, v, false);
          v.set(1.f);
          e->add(e->length / 2, v, false);
          file = dir.path() + "/Value looper-take1.values";
          const auto bytes = e->to_json();
          std::ofstream f{file.toStdString(), std::ios::binary};
          f.write(bytes.data(), std::streamsize(bytes.size()));
          length = 8000.;
        }
        REQUIRE(!file.isEmpty());

        // What the looper's view drags.
        Nodes::Looper::view_message m{};
        m.take = 1;
        const QString base = QFileInfo{file}.path() + "/" + QFileInfo{file}.completeBaseName();
        m.set_take(base.toStdString(), QFileInfo{file}.suffix().toStdString(), true, 0, stretch);
        Nodes::Looper::looper_view view;
        view.receive(m);
        std::unique_ptr<QMimeData> mime{
            oscr::make_drag_mime(view.drag_payload(view.width() - 20., 10.))};

        switch(w)
        {
          case where::send:
            oscr::sendToTimeline(
                *looper, doc->context(),
                halp::timeline_clip{
                    .files = {file.toStdString()}, .date = 1., .stretch = stretch});
            break;
          case where::new_box: {
            const auto& handlers = ctx.interfaces<Process::ProcessDropHandlerList>();
            auto res = handlers.getDrop(*mime, doc->context());
            REQUIRE(!res.empty());
            const auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));
            Scenario::Command::Macro mc{
                new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
            auto& b = mc.createBox(
                *scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(1000) + t, 0.6);
            score::Dispatcher_T disp{mc};
            for(auto& r : res)
              if(auto* p = mc.createProcessInNewSlot(b, r.creation); p && r.setup)
                r.setup(*p, disp);
            mc.commit();
            break;
          }
          case where::on_box:
            Scenario::DropProcessInInterval{}.drop(doc->context(), looper_box, {}, *mime);
            break;
        }
        QApplication::processEvents();

        std::vector<const Process::ProcessModel*> placed;
        for(auto& itv : scenario->intervals)
          for(auto& p : itv.processes)
            if(qobject_cast<const Media::Sound::ProcessModel*>(&p)
               || qobject_cast<const Automation::ProcessModel*>(&p))
              placed.push_back(&p);
        REQUIRE(placed.size() == 1);
        const auto& proc = *placed.front();
        const auto& box = *qobject_cast<const Scenario::IntervalModel*>(proc.parent());
        CHECK(proc.loops());
        CHECK(proc.loopDuration().msec() == Approx(length).margin(20.));
        if(w == where::on_box)
        {
          CHECK(&box == &looper_box);
          CHECK(proc.duration().msec() == Approx(20000.).margin(1.));
        }
        else
        {
          CHECK(box.duration.defaultDuration().msec() == Approx(length).margin(20.));
          CHECK(proc.duration().msec() == Approx(length).margin(20.));
        }

        if(auto* a = qobject_cast<const Automation::ProcessModel*>(&proc))
        {
          // The curve holds the take over the loop, from the start of the
          // process.
          const double loop = length / proc.duration().msec();
          const auto& c = a->curve();
          CHECK(*c.valueAt(0.1 * loop) == Approx(0.));
          CHECK(*c.valueAt(0.45 * loop) == Approx(0.));
          CHECK(*c.valueAt(0.55 * loop) == Approx(1.));
          CHECK(*c.valueAt(0.95 * loop) == Approx(1.));
        }
      });
    }
  }
}
