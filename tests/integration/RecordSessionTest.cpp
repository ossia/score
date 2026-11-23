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
#include <Scenario/Process/ScenarioModel.hpp>

#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <dr_wav.h>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <cmath>
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
