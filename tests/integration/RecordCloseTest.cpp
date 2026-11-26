// What happens to a recording when its document is closed or saved elsewhere:
// its takes are placed first, so the user is asked about a document that has
// them and the project's copy of them is whole.

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
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>
#include <score_test/Recording.hpp>

namespace
{
struct Recorded
{
  score::Document* doc{};
  Scenario::IntervalModel* box{};
  Process::ProcessModel* sound{};
  std::size_t processes{};
};

//! A box with a sound, recorded by the application's session, playing
Recorded startRecording(const score::GUIApplicationContext& ctx, const QString& ramp)
{
  Recorded r;
  r.doc = score::test::new_document(ctx);
  REQUIRE(r.doc);
  auto& scenar = score::test::base_scenario(*r.doc);
  {
    Scenario::Command::Macro m{
        new Scenario::Command::AddProcessInNewBoxMacro, r.doc->context()};
    auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
    r.box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
    r.sound = m.createProcessInNewSlot(
        *r.box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), ramp);
    m.commit();
  }
  REQUIRE(r.sound);
  r.processes = r.box->processes.size();
  // As a document just saved: nothing to ask about yet
  r.doc->commandStack().markCurrentIndexAsSaved();
  REQUIRE(r.doc->commandStack().isAtSavedIndex());

  auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
  REQUIRE(rec.startSession(r.doc->context(), {r.sound->outlets()[0]}, {}));
  auto& exec = r.doc->context().plugin<Execution::DocumentPlugin>();
  REQUIRE(score::test::wait_until([&] { return exec.isPlaying(); }));
  score::test::run_events_for(800);
  REQUIRE(rec.session());
  REQUIRE(rec.session()->running());
  return r;
}
}

TEST_CASE(
    "Closing a document that records places the take, then asks about saving it",
    "[recording][close][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    score::test::write_ramp_wav(ramp, rate, rate * 20);
    auto r = startRecording(ctx, ramp);
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();

    // The question about unsaved changes, answered "cancel": what the document
    // holds when it is asked
    bool asked = false;
    std::size_t processes_when_asked = 0;
    bool recording_when_asked = true;
    QTimer answer;
    answer.setInterval(20);
    QObject::connect(&answer, &QTimer::timeout, [&] {
      if(auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      {
        asked = true;
        processes_when_asked = r.box->processes.size();
        recording_when_asked = rec.session() != nullptr;
        box->done(QMessageBox::Cancel);
      }
    });
    answer.start();
    const bool closed = ctx.docManager.closeDocument(ctx, *r.doc);
    answer.stop();

    REQUIRE(asked);
    CHECK(!closed);
    CHECK(!recording_when_asked);
    CHECK(processes_when_asked == r.processes + 1);
    // Still there, with its take, after the cancel
    CHECK(r.box->processes.size() == r.processes + 1);
    CHECK(!r.doc->commandStack().isAtSavedIndex());

    ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution().request_stop();
    score::test::run_events_for(200);
  });
}

TEST_CASE(
    "Saving a document elsewhere while it records ends the recording, the take whole",
    "[recording][close][saveas][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString ramp = dir.filePath("ramp.wav");
    score::test::write_ramp_wav(ramp, rate, rate * 20);
    auto r = startRecording(ctx, ramp);
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    // What "Save as" tells the plug-ins before it writes the document
    const QString project = dir.filePath("project/saved.score");
    REQUIRE(QDir{}.mkpath(dir.filePath("project")));
    rec.on_documentSaveAs(*r.doc, project);
    r.doc->metadata().setFileName(project);

    // Over, placed, and the file the box plays is in the project and whole
    CHECK(!rec.session());
    REQUIRE(r.box->processes.size() == r.processes + 1);
    Media::Sound::ProcessModel* placed{};
    for(auto& p : r.box->processes)
      if(&p != r.sound)
        placed = qobject_cast<Media::Sound::ProcessModel*>(&p);
    REQUIRE(placed);
    const QString file = placed->file()->absoluteFileName();
    INFO(file.toStdString());
    CHECK(QFileInfo{file}.canonicalFilePath().startsWith(
        QFileInfo{dir.filePath("project")}.canonicalFilePath()));
    int channels = 0;
    const auto samples = score::test::read_wav(file, channels);
    REQUIRE(channels > 0);
    // More than half a second, every frame of it written
    CHECK(samples.size() / channels > std::size_t(rate / 2));
    // The sound's samples one after the other, to the last frame
    const std::size_t frames = samples.size() / channels;
    std::vector<std::size_t> gaps;
    // (past the fade-in of a sound that starts)
    for(std::size_t i = 1024; i < frames; i++)
      if(std::llround(samples[i * channels] * 1e6)
         != std::llround(samples[(i - 1) * channels] * 1e6) + 1)
        gaps.push_back(i);
    INFO("frames " << frames << ", " << gaps.size() << " gaps, first at "
                   << (gaps.empty() ? 0 : gaps[0]));
    CHECK(gaps.empty());

    ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution().request_stop();
    score::test::run_events_for(200);
  });
}
