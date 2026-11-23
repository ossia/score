// A texture outlet recorded to a video take: the frames of a shader, read back
// through the graphics context's preview path while the box plays, are
// encoded to a video file in Takes, which is placed in the box as a Video
// process starting where the recording started, in one undoable step.

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Gfx/Video/Process.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace
{
// A real event loop: the render loop posts events without end, which
// processEvents() alone may spend all its time on, starving the timers
void spin(int ms)
{
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
}

template <typename F>
bool spin_until(F f, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    spin(10);
  return f();
}

//! Seconds of video in a file, and its frame size
struct VideoInfo
{
  double seconds{};
  int width{}, height{};
};
VideoInfo probe(const QString& file)
{
  VideoInfo r;
  AVFormatContext* fmt{};
  if(avformat_open_input(&fmt, file.toStdString().c_str(), nullptr, nullptr) != 0)
    return r;
  avformat_find_stream_info(fmt, nullptr);
  for(unsigned i = 0; i < fmt->nb_streams; i++)
  {
    auto* st = fmt->streams[i];
    if(st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
      continue;
    r.width = st->codecpar->width;
    r.height = st->codecpar->height;
    if(fmt->duration > 0)
      r.seconds = double(fmt->duration) / AV_TIME_BASE;
  }
  avformat_close_input(&fmt);
  return r;
}
}

TEST_CASE("A texture outlet is recorded to a video take", "[recording][texture][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    Process::ProcessModel* shader{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
      shader = m.createProcessInNewSlot(
          *box, Metadata<ConcreteKey_k, Gfx::Filter::Model>::get(),
          QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-time-uniforms.fs"));
      m.commit();
    }
    REQUIRE(shader);
    REQUIRE(!shader->outlets().empty());
    const auto processes_before = box->processes.size();

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    // A GUI application's first playback can be stopped by the audio engine
    // starting up: play until it stays playing
    bool steady = false;
    for(int i = 0; i < 5 && !steady; i++)
    {
      if(!exec.isPlaying())
        engine.execution().request_play_global(true);
      spin(1500);
      steady = exec.isPlaying();
    }
    REQUIRE(steady);

    Recording::RecordSession session{doc->context(), {}};
    REQUIRE(session.addSource(*shader->outlets()[0]));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(2500);
    REQUIRE(!finished);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    const auto& take = session.takes()[0];
    INFO(take.file.toStdString());
    REQUIRE(QFile::exists(take.file));
    CHECK(QFileInfo{take.file}.suffix() == "mkv");

    // About the time recorded
    const auto info = probe(take.file);
    CHECK(info.width > 0);
    CHECK(info.height > 0);
    CHECK(info.seconds > 1.5);
    CHECK(info.seconds < 3.5);

    // A video process where the recording started
    REQUIRE(take.placed);
    CHECK(qobject_cast<Gfx::Video::Model*>(take.placed.data()));
    CHECK(take.placed->startOffset() == TimeVal{-take.startDate});
    CHECK(box->processes.size() == processes_before + 1);
    doc->commandStack().undo();
    CHECK(box->processes.size() == processes_before);
  });
}
