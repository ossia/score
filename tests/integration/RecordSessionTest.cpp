// A recording session from start to end, on a document that plays: a box
// plays a sound file whose every sample names its frame (a ramp); the
// session records the Sound process's outlet while the box plays.
//
// What has to hold:
//  - the take is a WAV file in the document's Takes folder, and holds the
//    file's samples from where the recording started, without a gap;
//  - it is placed in the box as a new process starting where the recording
//    started (start offset -start), in one undoable step: undo removes it,
//    redo puts it back;
//  - undo and redo are off while the session runs.

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/LiveTake.hpp>
#include <Process/ProcessList.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <score/model/ComponentUtils.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/MidiProcess.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Session/RecordSession.hpp>
#include <Recording/Takes/PlaceAddressTakes.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dr_wav.h>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <cmath>
#include <thread>
#include <vector>

namespace
{
constexpr double flicks_per_second = 705600000.;

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

//! 32-bit float mono, sample[i] = (i + 1) / 1e6: 0 is never a sample
void write_ramp(const QString& path, int rate, int frames)
{
  drwav_data_format fmt{};
  fmt.container = drwav_container_riff;
  fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  fmt.channels = 1;
  fmt.sampleRate = drwav_uint32(rate);
  fmt.bitsPerSample = 32;
  drwav wav;
  REQUIRE(drwav_init_file_write(&wav, path.toStdString().c_str(), &fmt, nullptr));
  std::vector<float> data(frames);
  for(int i = 0; i < frames; i++)
    data[i] = float((i + 1) * 1e-6);
  REQUIRE(drwav_write_pcm_frames(&wav, frames, data.data()) == drwav_uint64(frames));
  drwav_uninit(&wav);
}

std::vector<float> read_wav(const QString& path, int& channels)
{
  drwav wav;
  REQUIRE(drwav_init_file(&wav, path.toStdString().c_str(), nullptr));
  channels = wav.channels;
  std::vector<float> v(std::size_t(wav.totalPCMFrameCount) * wav.channels);
  drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, v.data());
  drwav_uninit(&wav);
  return v;
}
}

TEST_CASE("A recording session records a port while its box plays", "[recording][session]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);

    // A box of 10 s from the start, playing the ramp
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      // Hung off the scenario's start: a box nothing leads to does not play
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    REQUIRE(box);
    REQUIRE(sound);
    REQUIRE(!sound->outlets().empty());
    const auto processes_before = box->processes.size();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    engine.execution().request_play_global(true);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(300);

    Recording::RecordSession session{doc->context(), {}};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    // What the undo action shows
    bool undo_enabled = true;
    QObject::connect(
        &doc->commandStack(), &score::CommandStack::canUndoChanged,
        [&](bool b) { undo_enabled = b; });
    REQUIRE(session.start());
    CHECK(!undo_enabled);
    spin(800);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    CHECK(undo_enabled);
    CHECK(doc->commandStack().canUndo());

    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    const auto& take = session.takes()[0];
    INFO("take " << take.file.toStdString());
    const bool in_takes = QFileInfo{take.file}.dir().dirName() == "Takes"
                          || take.file.contains("/recordings/");
    CHECK(in_takes);
    REQUIRE(QFile::exists(take.file));

    // The file's samples from where the recording started, without a gap
    int channels{};
    const auto rec = read_wav(take.file, channels);
    REQUIRE(channels >= 1);
    const std::size_t frames = rec.size() / channels;
    CHECK(frames > std::size_t(rate / 2));
    const int64_t start_frame
        = std::llround(take.startDate / flicks_per_second * rate);
    // Which ramp sample the first recorded frame is
    const int64_t first = std::llround(rec[0] * 1e6) - 1;
    CHECK(std::abs(first - start_frame) <= 2);
    int64_t gaps = 0;
    for(std::size_t i = 1; i < frames; i++)
      gaps += std::llround(rec[i * channels] * 1e6) != std::llround(rec[(i - 1) * channels] * 1e6) + 1;
    CHECK(gaps == 0);

    // Placed in the box, starting where it was recorded
    REQUIRE(take.placed);
    CHECK(take.placed->parent() == box);
    CHECK(take.placed->startOffset() == TimeVal{-take.startDate});
    CHECK(!take.placed->loops());
    CHECK(box->processes.size() == processes_before + 1);

    // One undoable step
    doc->commandStack().undo();
    CHECK(box->processes.size() == processes_before);
    doc->commandStack().redo();
    CHECK(box->processes.size() == processes_before + 1);
  });
}

TEST_CASE("A recording started while stopped starts playback and records", "[recording][session]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    REQUIRE(sound);

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    REQUIRE(!exec.isPlaying());

    Recording::RecordSession session{doc->context(), {}};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(800);

    // Stopping playback ends the recording
    engine.execution().request_stop();
    REQUIRE(spin_until([&] { return finished; }));

    REQUIRE(session.takes().size() == 1);
    const auto& take = session.takes()[0];
    REQUIRE(QFile::exists(take.file));
    int channels{};
    const auto rec = read_wav(take.file, channels);
    CHECK(rec.size() / channels > std::size_t(rate / 4));
    REQUIRE(take.placed);
    CHECK(take.placed->parent() == box);
  });
}

TEST_CASE("The Record button records the selected port and stops", "[recording][session]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    REQUIRE(sound);
    const auto processes_before = box->processes.size();

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();

    // Nothing selected: nothing records
    score::SelectionDispatcher{doc->context().selectionStack}.deselect();
    rec.recordSelection();
    CHECK(!rec.session());

    score::SelectionDispatcher{doc->context().selectionStack}.select(*sound->outlets()[0]);
    rec.recordSelection();
    auto* session = rec.session();
    REQUIRE(session);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(600);
    bool finished = false;
    QObject::connect(session, &Recording::RecordSession::finished, [&] { finished = true; });
    // Again: it stops
    rec.recordSelection();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);
    CHECK(box->processes.size() == processes_before + 1);
    CHECK(spin_until([&] { return rec.session() == nullptr; }));
  });
}

TEST_CASE("score:/recording records and stops as the Record button does", "[recording][session][remote]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    REQUIRE(sound);
    const auto processes_before = box->processes.size();

    auto* lt = doc->context().findPlugin<LocalTree::DocumentPlugin>();
    REQUIRE(lt);
    auto& root = lt->device().get_root_node();
    auto* record = ossia::net::find_node(root, "/recording/record");
    auto* stop = ossia::net::find_node(root, "/recording/stop");
    auto* active = ossia::net::find_node(root, "/recording/active");
    REQUIRE(record);
    REQUIRE(stop);
    REQUIRE(active);
    CHECK(active->get_parameter()->value() == ossia::value{false});

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    score::SelectionDispatcher{doc->context().selectionStack}.select(*sound->outlets()[0]);

    // As a message from the network: from another thread
    std::thread{[&] { record->get_parameter()->push_value(ossia::impulse{}); }}.join();
    REQUIRE(spin_until([&] { return rec.session() != nullptr; }));
    auto* session = rec.session();
    bool finished = false;
    QObject::connect(session, &Recording::RecordSession::finished, [&] { finished = true; });
    CHECK(spin_until([&] { return active->get_parameter()->value() == ossia::value{true}; }));
    spin(600);

    std::thread{[&] { stop->get_parameter()->push_value(ossia::impulse{}); }}.join();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);
    CHECK(box->processes.size() == processes_before + 1);
    CHECK(spin_until([&] { return active->get_parameter()->value() == ossia::value{false}; }));
  });
}

TEST_CASE("A MIDI recording into a piano roll adds its notes there, undoably", "[recording][session][overdub]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Midi::ProcessModel* source{};
    Midi::ProcessModel* target{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      source = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
      target = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
      REQUIRE(source);
      REQUIRE(target);
      // A note every 100 ms, pitch 70, over the whole box
      std::vector<Midi::NoteData> notes;
      for(int i = 0; i < 100; i++)
        notes.emplace_back(i / 100., 0.005, 70, 100);
      m.submit(new Midi::ReplaceNotes{*source, notes, 0, 127, source->duration()});
      m.commit();
    }
    REQUIRE(target->notes.size() == 0);

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.target = Recording::RecordOptions::Target::Into;
    opt.into = target;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*source->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(1200);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    // The notes heard, in the piano roll; nothing new in the box
    CHECK(box->processes.size() == 2);
    REQUIRE(target->notes.size() >= 5);
    for(auto& n : target->notes)
    {
      CHECK(n.pitch() == 70);
      // On the 100 ms grid of the source
      const double at = n.start() * 100.;
      CHECK(std::abs(at - std::round(at)) < 0.05);
    }
    doc->commandStack().undo();
    CHECK(target->notes.size() == 0);
    doc->commandStack().redo();
    CHECK(target->notes.size() >= 5);
  });
}

TEST_CASE("Values recorded into an automation replace its curve, and it is silent meanwhile", "[recording][session][overdub]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Automation::ProcessModel* source{};
    Automation::ProcessModel* target{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      source = static_cast<Automation::ProcessModel*>(m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}));
      target = static_cast<Automation::ProcessModel*>(m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}));
      REQUIRE(source);
      REQUIRE(target);
      // The source goes from 10 to 20
      m.submit(new Automation::SetMin{*source, 10.});
      m.submit(new Automation::SetMax{*source, 20.});
      m.commit();
    }
    REQUIRE(target->max() == 1.);

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.target = Recording::RecordOptions::Target::Into;
    opt.into = target;
    opt.replace = true;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*source->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(1000);

    // Silent in the execution, not in the document
    auto* comp = score::findComponent<Execution::ProcessComponent>(target->components());
    REQUIRE(comp);
    CHECK(comp->OSSIAProcess().muted_by_self());
    CHECK(!target->muted());

    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    spin(100);
    CHECK(!comp->OSSIAProcess().muted_by_self());
    engine.execution().request_stop();
    spin(200);

    // The values the source played, in the target's curve
    CHECK(box->processes.size() == 2);
    CHECK(target->max() >= 10.);
    CHECK(target->max() <= 20.);
    REQUIRE(session.takes().size() == 1);
    CHECK(session.takes()[0].placed == target);

    doc->commandStack().undo();
    CHECK(target->max() == 1.);
    doc->commandStack().redo();
    CHECK(target->max() >= 10.);
  });
}

TEST_CASE("A take in passes: alternatives in one slot, the last playing, each turned to the box", "[recording][session][passes]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 30);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    REQUIRE(sound);
    const auto processes_before = box->processes.size();
    const auto slots_before = box->smallView().size();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    // Passes of a bar: 2 s at 120 in 4/4, from 0.5 s on
    engine.execution().request_play_global(true);
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(500);

    Recording::RecordOptions opt;
    opt.passes = Recording::RecordOptions::Passes::Alternatives;
    opt.passBars = 1.;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(4800);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    const auto take = session.takes()[0];
    const int64_t bar = 2 * 705600000;
    const auto passes = (take.endDate - take.startDate) / bar;
    REQUIRE(passes >= 2);

    // A process each, in one new slot, looping a bar, the last one unmuted
    REQUIRE(box->processes.size() == processes_before + passes);
    CHECK(box->smallView().size() == slots_before + 1);
    std::vector<Process::ProcessModel*> added;
    for(auto& p : box->processes)
      if(&p != sound)
        added.push_back(&p);
    int unmuted = 0;
    for(auto* p : added)
    {
      CHECK(p->loops());
      CHECK(p->loopDuration() == TimeVal{bar});
      CHECK(p->startOffset() == TimeVal::zero());
      unmuted += !p->muted();
    }
    CHECK(unmuted == 1);
    CHECK(!take.placed->muted());

    // The last pass, turned: at each frame of the loop, the ramp frame played
    // at that moment of the box during that pass
    auto* last = static_cast<Media::Sound::ProcessModel*>(take.placed.data());
    const QString file = last->file()->absoluteFileName();
    INFO(file.toStdString());
    int channels{};
    const auto rec = read_wav(file, channels);
    const int64_t loop_frames = 2 * rate;
    REQUIRE(int64_t(rec.size() / channels) == loop_frames);
    const int64_t pass_start = take.startDate + (passes - 1) * bar;
    const int64_t pass_start_frame = std::llround(pass_start / flicks_per_second * rate);
    for(int64_t i : {int64_t(0), loop_frames / 3, loop_frames - 1})
    {
      // Box frame of loop position i during the pass
      const int64_t r = ((pass_start_frame % loop_frames) + loop_frames) % loop_frames;
      const int64_t box_frame = pass_start_frame + ((i - r + loop_frames) % loop_frames);
      const int64_t heard = std::llround(rec[i * channels] * 1e6) - 1;
      CHECK(std::abs(heard - box_frame) <= 2);
    }

    doc->commandStack().undo();
    CHECK(box->processes.size() == processes_before);
  });
}

TEST_CASE("A take in passes side by side: each where it was played", "[recording][session][passes]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 30);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    const auto processes_before = box->processes.size();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.passes = Recording::RecordOptions::Passes::SideBySide;
    opt.passBars = 1.;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(4500);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    const auto& take = session.takes().at(0);
    const int64_t bar = 2 * 705600000;
    const auto passes = (take.endDate - take.startDate) / bar;
    REQUIRE(passes >= 2);
    REQUIRE(box->processes.size() == processes_before + passes);
    std::vector<int64_t> offsets;
    for(auto& p : box->processes)
    {
      if(&p == sound)
        continue;
      CHECK(!p.loops());
      CHECK(!p.muted());
      offsets.push_back(-p.startOffset().impl);
    }
    std::sort(offsets.begin(), offsets.end());
    for(int64_t k = 0; k < passes; k++)
      CHECK(offsets[k] == take.startDate + k * bar);
  });
}

TEST_CASE("Saved for the first time, a document keeps its takes in its Takes folder", "[recording][session][files]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordSession session{doc->context(), {}};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(600);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    const auto& take = session.takes().at(0);
    REQUIRE(take.placed);
    const QString before = take.file;
    REQUIRE(QFile::exists(before));
    CHECK(before.contains("/recordings/"));

    // What saving does, before the document knows its new name
    const QString project = dir.filePath("project/My project.score");
    QDir{}.mkpath(dir.filePath("project"));
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    rec.on_documentSaveAs(*doc, project);
    doc->metadata().setFileName(project);

    const QString copied = dir.filePath("project/Takes/") + QFileInfo{before}.fileName();
    CHECK(QFile::exists(copied));
    CHECK(QFile::exists(before));
    auto* placed = static_cast<Media::Sound::ProcessModel*>(take.placed.data());
    INFO("original " << placed->file()->originalFile().toStdString() << " abs "
                     << placed->file()->absoluteFileName().toStdString() << " before "
                     << before.toStdString());
    CHECK(QFileInfo{placed->file()->absoluteFileName()}.canonicalFilePath()
          == QFileInfo{copied}.canonicalFilePath());

    // Undoable, and saved again nothing more happens
    doc->commandStack().undo();
    CHECK(QFileInfo{placed->file()->absoluteFileName()}.canonicalFilePath()
          == QFileInfo{before}.canonicalFilePath());
    doc->commandStack().redo();
    const int index = doc->commandStack().currentIndex();
    rec.on_documentSaveAs(*doc, dir.filePath("project/Other.score"));
    CHECK(doc->commandStack().currentIndex() == index);
  });
}

TEST_CASE("A recording counted in clicks a whole bar, then starts on a bar line", "[recording][session][countin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 40);

    // A box of the scenario, and the document's root box, which has no
    // component of its own
    for(auto [playing, root] : {std::pair{true, false}, {false, false}, {true, true}, {false, true}})
    {
      INFO((playing ? "while playing" : "from a stop") << (root ? ", in the root box" : ", in a box"));
      auto* doc = score::test::new_document(ctx);
      REQUIRE(doc);
      auto& scenar = score::test::base_scenario(*doc);
      Process::ProcessModel* sound{};
      {
        Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
        Scenario::IntervalModel* box = &score::test::base_interval(*doc);
        if(!root)
        {
          auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
          box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
        }
        sound = m.createProcessInNewSlot(
            *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
        m.commit();
      }
      auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
      auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
      if(playing)
      {
        bool steady = false;
        for(int i = 0; i < 5 && !steady; i++)
        {
          if(!exec.isPlaying())
            engine.execution().request_play_global(true);
          spin(1500);
          steady = exec.isPlaying();
        }
        REQUIRE(steady);
        // Just after a bar line: the next one is too close to count in
        spin(2000 - 0 + 100);
      }
      else
      {
        // The audio engine has started and settled, and playback is stopped
        engine.execution().request_play_global(true);
        spin(1500);
        engine.execution().request_stop();
        REQUIRE(spin_until([&] { return !exec.isPlaying(); }));
        spin(500);
      }

      Recording::RecordOptions opt;
      opt.startRate = 1.;
      opt.countIn = true;
      Recording::RecordSession session{doc->context(), opt};
      REQUIRE(session.addSource(*sound->outlets()[0]));
      bool finished = false;
      QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
      QElapsedTimer clock;
      clock.start();
      REQUIRE(session.start());
      // It waits a whole bar at least, clicking
      REQUIRE(spin_until([&] { return session.running() && !session.waiting(); }, 8000));
      const qint64 waited = clock.elapsed();
      const int beats = session.countInBeats();
      CHECK(waited >= 1800);
      CHECK(waited <= 4500);
      CHECK(beats >= 4);
      CHECK(beats <= 9);
      spin(1000);
      // No more clicks while it records
      CHECK(session.countInBeats() == beats);
      session.stop();
      REQUIRE(spin_until([&] { return finished; }));
      engine.execution().request_stop();
      spin(300);

      REQUIRE(session.takes().size() == 1);
      // On a bar: 2 s at 120 in 4/4, within a sample, and not the first one
      const int64_t bar = 2 * 705600000;
      const int64_t off = session.takes()[0].startDate % bar;
      CHECK(std::min(off, bar - off) <= 705600000 / 48000);
      CHECK(session.takes()[0].startDate >= bar - 705600000 / 48000);
      CHECK(session.warnings().isEmpty());
    }
  });
}

TEST_CASE("Muting what was recorded mutes the source with the take placed", "[recording][session]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.muteSource = true;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(800);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().at(0).placed);
    CHECK(sound->muted());
    CHECK(!session.takes()[0].placed->muted());
    // The same step
    doc->commandStack().undo();
    CHECK(!sound->muted());
    CHECK(box->processes.size() == 1);
  });
}

namespace
{
//! A sound of the ramp directly in the root interval, as processes of a
//! document's main node view are
Process::ProcessModel* rootSound(score::Document& doc, const QString& ramp)
{
  auto& root = score::test::base_interval(doc);
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  auto* p = m.createProcessInNewSlot(
      root, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
  m.commit();
  return p;
}
}

TEST_CASE("Bars from the next bar, from a stop: the first bar, as a loop", "[recording][session][bars]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 30);
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* sound = rootSound(*doc, ramp);
    REQUIRE(sound);
    auto& root = score::test::base_interval(*doc);
    const auto before = root.processes.size();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.startRate = 1.;
    opt.lengthBars = 1.;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    REQUIRE(spin_until([&] { return finished; }, 8000));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    const auto& take = session.takes()[0];
    // The first bar: playback starts on it
    CHECK(take.startDate == 0);
    REQUIRE(take.placed);
    REQUIRE(root.processes.size() == before + 1);
    const int64_t bar = 2 * 705600000;
    CHECK(take.placed->loops());
    CHECK(take.placed->loopDuration() == TimeVal{bar});
    CHECK(take.placed->startOffset() == TimeVal::zero());

    // Its content is the bar played: the ramp from its start
    auto* placed = static_cast<Media::Sound::ProcessModel*>(take.placed.data());
    int channels{};
    const auto rec = read_wav(placed->file()->absoluteFileName(), channels);
    REQUIRE(int64_t(rec.size() / channels) == 2 * rate);
    for(int64_t i : {int64_t(0), int64_t(rate), int64_t(2 * rate - 1)})
      CHECK(std::abs(std::llround(rec[i * channels] * 1e6) - 1 - i) <= 2);
  });
}

TEST_CASE("Bars from the next bar while playing long: a loop from 0, turned to the bars", "[recording][session][bars]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 30);
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* sound = rootSound(*doc, ramp);
    REQUIRE(sound);

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    engine.execution().request_play_global(true);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    // Well into the third bar
    spin(5300);

    Recording::RecordOptions opt;
    opt.startRate = 1.;
    opt.lengthBars = 1.;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    REQUIRE(spin_until([&] { return finished; }, 8000));
    engine.execution().request_stop();
    spin(200);

    const auto& take = session.takes().at(0);
    const int64_t bar = 2 * 705600000;
    CHECK(take.startDate >= 3 * bar - 705600000 / rate);
    REQUIRE(take.placed);
    CHECK(take.placed->loops());
    CHECK(take.placed->loopDuration() == TimeVal{bar});
    CHECK(take.placed->startOffset() == TimeVal::zero());

    // At each moment of a bar, the ramp frame played then during the take
    auto* placed = static_cast<Media::Sound::ProcessModel*>(take.placed.data());
    int channels{};
    const auto rec = read_wav(placed->file()->absoluteFileName(), channels);
    const int64_t loop = 2 * rate;
    REQUIRE(int64_t(rec.size() / channels) == loop);
    const int64_t s = std::llround(take.startDate / flicks_per_second * rate);
    const int64_t r = s % loop;
    for(int64_t i : {int64_t(0), loop / 3, loop - 1})
    {
      const int64_t heard = std::llround(rec[i * channels] * 1e6) - 1;
      CHECK(std::abs(heard - (s + ((i - r + loop) % loop))) <= 2);
    }
  });
}

TEST_CASE("Bars of MIDI from a cable while playing long: a one-bar piano roll loop from 0", "[recording][session][bars]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& root = score::test::base_interval(*doc);
    Midi::ProcessModel* source{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      source = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
          root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
      REQUIRE(source);
      // A note every quarter, over the whole process
      std::vector<Midi::NoteData> notes;
      const int quarters = int(source->duration().impl / 352800000);
      for(int i = 0; i < quarters; i++)
        notes.emplace_back(double(i) / quarters, 0.25 / quarters, 60 + (i % 4), 100);
      m.submit(new Midi::ReplaceNotes{*source, notes, 0, 127, source->duration()});
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    engine.execution().request_play_global(true);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(3300);

    Recording::RecordOptions opt;
    opt.startRate = 1.;
    opt.lengthBars = 1.;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*source->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    REQUIRE(spin_until([&] { return finished; }, 8000));
    engine.execution().request_stop();
    spin(200);

    const auto& take = session.takes().at(0);
    REQUIRE(take.placed);
    auto* midi = qobject_cast<Midi::ProcessModel*>(take.placed.data());
    REQUIRE(midi);
    const int64_t bar = 2 * 705600000;
    CHECK(midi->loops());
    CHECK(midi->loopDuration() == TimeVal{bar});
    CHECK(midi->startOffset() == TimeVal::zero());
    // Four quarter notes, spread over the bar, not squashed
    REQUIRE(midi->notes.size() == 4);
    std::vector<double> starts;
    for(auto& n : midi->notes)
      starts.push_back(n.start() * midi->duration().impl / 352800000.);
    std::sort(starts.begin(), starts.end());
    for(int i = 0; i < 4; i++)
      CHECK(std::abs(starts[i] - i) < 0.05);
  });
}

TEST_CASE("An inlet's take plays into it, placed where asked", "[recording][session][inlet]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    auto& model = doc->model().modelDelegate();
    auto& dm = static_cast<Scenario::ScenarioDocumentModel&>(model);

    // A gain, its audio inlet fed by a sound
    UuidKey<Process::ProcessModel> gain_key{};
    for(auto& f : ctx.interfaces<Process::ProcessFactoryList>())
      if(f.prettyName() == "Gain")
        gain_key = f.concreteKey();
    if(gain_key == UuidKey<Process::ProcessModel>{})
      SKIP("no Gain process in this build");

    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    Process::ProcessModel* gain{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      gain = m.createProcessInNewSlot(*box, gain_key, {});
      REQUIRE(sound);
      REQUIRE(gain);
      m.createCable(dm, *sound->outlets()[0], *gain->inlets()[0], Process::CableType::ImmediateGlutton);
      m.commit();
    }
    const auto cables_before = dm.cables.size();
    const auto processes_before = box->processes.size();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.nodePosition = QPointF{123., 856.};
    opt.muteSource = true;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*gain->inlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(800);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    auto* take = session.takes()[0].placed.data();
    REQUIRE(take);
    REQUIRE(box->processes.size() == processes_before + 1);
    // Where asked
    CHECK(take->position() == QPointF{123., 856.});
    // Another node is never put on top of it
    const auto next = Recording::freeNodePosition(*box, QPointF{123., 856.});
    CHECK(!QRectF(next, Recording::takeNodeSize)
                .intersects(QRectF(take->position(), Recording::takeNodeSize)));
    // Into the inlet it was recorded from
    REQUIRE(dm.cables.size() == cables_before + 1);
    bool into_gain = false;
    for(auto& c : dm.cables)
      if(c.source().try_find(doc->context()) == take->outlets()[0]
         && c.sink().try_find(doc->context()) == gain->inlets()[0])
        into_gain = true;
    CHECK(into_gain);
    // What fed the inlet is muted, not the gain
    CHECK(sound->muted());
    CHECK(!gain->muted());

    doc->commandStack().undo();
    CHECK(dm.cables.size() == cables_before);
    CHECK(box->processes.size() == processes_before);
    CHECK(!sound->muted());
  });
}

TEST_CASE("What is being recorded is shown in its box while it records", "[recording][session][live]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    write_ramp(ramp, rate, rate * 20);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
      REQUIRE(sound);
      m.commit();
    }

    auto& live = Process::LiveTakes::instance();
    REQUIRE(live.takes().empty());
    int added = 0, removed = 0, changes = 0;
    auto c1 = QObject::connect(&live, &Process::LiveTakes::added, [&](Process::LiveTake* t) {
      added++;
      QObject::connect(t, &Process::LiveTake::changed, [&] { changes++; });
    });
    auto c2 = QObject::connect(
        &live, &Process::LiveTakes::removed, [&](Process::LiveTake*) { removed++; });

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.nodePosition = QPointF{10., 20.};
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    CHECK(added == 1);
    REQUIRE(live.takes().size() == 1);
    auto* t = live.takes()[0];
    CHECK(t->interval == box);
    CHECK(t->kind == Process::LiveTake::Audio);
    // Where the take will be placed: the first free spot from the asked one
    REQUIRE(t->nodePosition);
    const QPointF shown = *t->nodePosition;
    CHECK(shown == Recording::takeNodePosition(*box, QPointF{10., 20.}));
    CHECK(t->newProcess);

    REQUIRE(spin_until([&] { return t->recording && t->end - t->start > 705600000 / 2; }, 5000));
    // The ramp's samples, seen as they are recorded
    CHECK(!t->peaks.empty());
    CHECK(*std::max_element(t->peaks.begin(), t->peaks.end()) > 0.01f);
    const auto peaks_then = t->peaks.size();
    const auto end_then = t->end;
    spin(300);
    CHECK(t->peaks.size() > peaks_then);
    CHECK(t->end > end_then);
    CHECK(changes > 2);

    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);
    // Gone once the take is a process
    CHECK(removed == 1);
    CHECK(live.takes().empty());
    QObject::disconnect(c1);
    QObject::disconnect(c2);
  });
}

TEST_CASE("Values recorded into an automation where touched: it plays until touched, and comes back after", "[recording][session][overdub][touch]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Automation::ProcessModel* source{};
    Automation::ProcessModel* target{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      source = static_cast<Automation::ProcessModel*>(m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}));
      target = static_cast<Automation::ProcessModel*>(m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}));
      REQUIRE(source);
      REQUIRE(target);
      m.submit(new Automation::SetMin{*source, 10.});
      m.submit(new Automation::SetMax{*source, 20.});
      m.commit();
    }
    const auto valueAt = [&](double x) {
      for(auto* seg : target->curve().sortedSegments())
        if(x >= seg->start().x() && x <= seg->end().x())
          return target->min() + seg->valueAt(x) * (target->max() - target->min());
      return -1.;
    };
    const double end_before = valueAt(1.);

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.target = Recording::RecordOptions::Target::Into;
    opt.into = target;
    opt.replace = true;
    opt.touch = true;
    opt.touchRelease = 705600000 / 4; // 250 ms
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*source->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    auto* comp = score::findComponent<Execution::ProcessComponent>(target->components());
    auto* src = score::findComponent<Execution::ProcessComponent>(source->components());
    REQUIRE(comp);
    REQUIRE(src);

    // The source sends: the target is silent
    REQUIRE(spin_until([&] { return comp->OSSIAProcess().muted_by_self(); }, 3000));
    spin(600);
    // The source stops: the target plays again after the release
    exec.context().executionQueue.enqueue([p = src->OSSIAProcessPtr()] { p->mute(true); });
    REQUIRE(spin_until([&] { return !comp->OSSIAProcess().muted_by_self(); }, 2000));
    spin(1500);
    CHECK(!comp->OSSIAProcess().muted_by_self());

    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    CHECK(session.takes()[0].placed == target);
    // The values where touched
    CHECK(target->max() >= 10.);
    // Its own curve after the release: the end of the box is as it was
    CHECK(valueAt(1.) == Catch::Approx(end_before).margin(1e-6));
    // ...and right before the box's end too, past the touched part
    CHECK(valueAt(0.9) < 10.);

    doc->commandStack().undo();
    CHECK(target->max() == 1.);
  });
}
