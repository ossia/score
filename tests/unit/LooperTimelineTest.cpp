// A looper's take sent to the timeline becomes a box of the scenario that
// holds the looper, at the date the take was recorded, in one undoable step.

#include <Process/Process.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/ProcessList.hpp>

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
#include <QTemporaryDir>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/HostServices.hpp>
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
    node.inputs.quantif.value = 0.f;
    node.inputs.late.value = 0;
    node.inputs.grace.value = 0;
    node.inputs.postaction_bars.value = 0;
    node.inputs.passthrough = MidiLooper::Passthrough::None;
    node.inputs.restart = MidiLooper::Restart::Recording;
    node.inputs.takes.value = files;
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
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
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
    DYNAMIC_SECTION("Takes " << int(files))
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
          looper.inputs.quantif.value = 0.f;
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
          .replaced_by = -1};
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
