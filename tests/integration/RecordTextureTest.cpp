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
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/Qml/EditContext.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>

#include <QQmlEngine>

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
#include <libavcodec/avcodec.h>
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
  std::string codec;
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
    r.codec = avcodec_get_name(st->codecpar->codec_id);
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
    // The size of the settings, not a thumbnail's
    CHECK(info.width == 1920);
    CHECK(info.height == 1080);
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

TEST_CASE("A camera selected in the explorer is recorded to a video take", "[recording][texture][camera][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    constexpr auto camera_uuid = "d615690b-f2e2-447b-b70e-a800552db69c";
    if(!ctx.interfaces<Device::ProtocolFactoryList>().get(
           UuidKey<Device::ProtocolFactory>::fromString(QString{camera_uuid})))
      SKIP("no Camera protocol in this build");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // A real camera, as the explorer's "Add device" lists them
    QQmlEngine js;
    auto* api = new JS::EditJsContext;
    js.globalObject().setProperty("Score", js.newQObject(api));
    js.evaluate(
        QString{"var e = Score.enumerateDevices(\"%1\"); e.enumerate = true;"}.arg(camera_uuid));
    const int n = js.evaluate("e.devices.length").toInt();
    int pick = -1;
    for(int i = 0; i < n && pick < 0; i++)
    {
      const auto name = js.evaluate(QString{"e.devices[%1].name"}.arg(i)).toString();
      const auto cat = js.evaluate(QString{"e.devices[%1].category"}.arg(i)).toString();
      const QString both = name + " " + cat;
      if(both.contains("1280x720") && !both.contains("Default"))
        pick = i;
    }
    if(pick < 0)
      SKIP("no camera with a 1280x720 mode here");
    js.evaluate(
        QString{"Score.createDevice(\"Cam\", \"%1\", e.devices[%2].settings)"}
            .arg(camera_uuid)
            .arg(pick));
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    REQUIRE(spin_until([&] {
      auto* d = plug.list().findDevice("Cam");
      return d && d->connected() && d->getDevice()
             && d->getDevice()->get_root_node().get_parameter();
    }));

    // A GUI application's first playback can be stopped by the audio engine
    // starting up: play until it stays playing
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    bool steady = false;
    for(int i = 0; i < 5 && !steady; i++)
    {
      if(!exec.isPlaying())
        engine.execution().request_play_global(true);
      spin(1500);
      steady = exec.isPlaying();
    }
    REQUIRE(steady);

    // Recorded as the explorer's Record menu does: the device, into the main box
    auto& box = score::test::base_interval(*doc);
    const auto before = box.processes.size();
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto* session = rec.recordAddressesInto(
        box, {State::Address{"Cam", {}}}, Recording::AddressPlacement::Automations);
    REQUIRE(session);
    REQUIRE(session->sources() == 1);
    bool finished = false;
    std::vector<Recording::RecordSession::Take> takes;
    QObject::connect(session, &Recording::RecordSession::finished, [&] {
      finished = true;
      takes = session->takes();
    });
    spin(3000);
    REQUIRE(!finished);
    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(takes.size() == 1);
    INFO(takes[0].file.toStdString());
    REQUIRE(QFile::exists(takes[0].file));
    const auto info = probe(takes[0].file);
    CHECK(info.width == 1920);
    CHECK(info.height == 1080);
    CHECK(info.seconds > 1.0);
    REQUIRE(box.processes.size() == before + 1);
    bool video = false;
    for(auto& p : box.processes)
      video = video || qobject_cast<Gfx::Video::Model*>(&p);
    CHECK(video);
  });
}

TEST_CASE("Video takes are encoded with the codec and size asked", "[recording][texture][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Process::ProcessModel* shader{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      auto& box = m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
      shader = m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Gfx::Filter::Model>::get(),
          QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-time-uniforms.fs"));
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    bool steady = false;
    for(int i = 0; i < 5 && !steady; i++)
    {
      if(!exec.isPlaying())
        engine.execution().request_play_global(true);
      spin(1500);
      steady = exec.isPlaying();
    }
    REQUIRE(steady);

    const std::pair<Recording::VideoSettings::Codec, const char*> codecs[]{
        {Recording::VideoSettings::Hap, "hap"}, {Recording::VideoSettings::Ffv1, "ffv1"}};
    for(auto [codec, name] : codecs)
    {
      Recording::RecordOptions opt;
      opt.target = Recording::RecordOptions::Target::FileOnly;
      opt.video.codec = codec;
      opt.video.size = {640, 360};
      opt.video.fps = 25;
      Recording::RecordSession session{doc->context(), opt};
      REQUIRE(session.addSource(*shader->outlets()[0]));
      bool finished = false;
      QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
      REQUIRE(session.start());
      spin(1500);
      session.stop();
      REQUIRE(spin_until([&] { return finished; }));
      REQUIRE(session.takes().size() == 1);
      const auto info = probe(session.takes()[0].file);
      INFO(session.takes()[0].file.toStdString());
      CHECK(info.codec == name);
      CHECK(info.width == 640);
      CHECK(info.height == 360);
    }
    engine.execution().request_stop();
    spin(200);
  });
}
