// CalibrationRun without a GPU: the simulated bench films what the run shows,
// through a fake camera and display, on a clock and an executor of the test.
#include "StructuredLightBench.hpp"

#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CalibrationRun.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace Threedim;
using namespace slbench;

namespace
{
struct Rig
{
  Bench bench{2};
  double now{};
  CalibrationRun* run{};
  std::vector<std::function<void()>> jobs;
  std::function<bool()> hold;

  int windows{2};
  CalibrationRun::Images shown;
  int shows{}, clears{};

  std::optional<Gfx::port_index> producer{Gfx::port_index{7, 0}};
  std::vector<Gfx::port_index> producers;
  std::vector<bool> streaming;
  std::vector<float> exposures;
  std::vector<bool> autoExposures;
  Gfx::TextureCapture::State cameraState{Gfx::TextureCapture::State::Capturing};
  QSize frameSize{CW, CH};

  struct FakeCamera final : CalibrationRun::Camera
  {
    Rig& rig;
    bool positions{};
    bool requested{};
    int64_t count{};
    FakeCamera(Rig& r, bool p)
        : rig{r}
        , positions{p}
    {
    }
    Gfx::TextureCapture::State state() const override { return rig.cameraState; }
    QString reason() const override { return QStringLiteral("unplugged"); }
    void setProducer(Gfx::port_index p) override { rig.producers.push_back(p); }
    void request() override { requested = true; }
    bool take(std::vector<float>& rgba, int& w, int& h) override
    {
      if(!requested || !rig.run)
        return false;
      if(positions)
      {
        rgba = rig.bench.positions;
        w = CW;
        h = CH;
        requested = false;
        count++;
        return true;
      }
      const float* f = rig.bench.capture(rig.run->session(), rig.now);
      if(!f)
        return false;
      w = rig.frameSize.width();
      h = rig.frameSize.height();
      rgba.assign(f, f + std::size_t(w) * h * 4);
      requested = false;
      count++;
      return true;
    }
    int64_t frames() const override { return count; }
  };

  CalibrationRun::Setup setup(const QString& base)
  {
    bench.noise = 0.002f;
    CalibrationRun::Setup s;
    s.display.windows = [this] { return windows; };
    s.display.show = [this](CalibrationRun::Images i) {
      shown = std::move(i);
      shows++;
    };
    s.display.clear = [this] { clears++; };
    s.camera = [this] { return producer; };
    s.positions = [] { return std::optional<Gfx::port_index>{Gfx::port_index{8, 0}}; };
    s.openCamera = [this](Gfx::port_index p) -> std::unique_ptr<CalibrationRun::Camera> {
      return std::make_unique<FakeCamera>(*this, p.node == 8);
    };
    s.keepStreaming = [this](bool on) { streaming.push_back(on); };
    s.executor = [this](std::function<void()> j) { jobs.push_back(std::move(j)); };
    s.setExposure = [this](float v) { exposures.push_back(v); };
    s.setAutoExposure = [this](bool v) { autoExposures.push_back(v); };
    s.reopenDelaysMs = {300., 600.};
    s.warmupMs = 1000.;

    Gfx::WindowSettings ws;
    ws.mode = Gfx::WindowMode::MultiWindow;
    for(int i = 0; i < 2; i++)
    {
      Gfx::OutputMapping m;
      m.windowSize = {PW, PH};
      m.sourceRect = {0.5 * i, 0, 0.5, 1};
      ws.outputs.push_back(m);
    }
    s.plan = planCalibration(ws, base);
    s.settings = bench.settings();
    return s;
  }

  void tick(int ticks = 1)
  {
    for(int i = 0; i < ticks && run->running(); i++)
    {
      run->tick(now);
      now += 16.7;
      if(hold && hold())
        continue;
      for(std::size_t k = 0; k < jobs.size(); k++)
        std::exchange(jobs[k], {})();
      jobs.clear();
    }
  }
};
}

TEST_CASE("CalibrationRun calibrates two outputs on the bench", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  auto s = rig.setup(dir.filePath("hall"));
  s.originalExposure = 7.f;
  s.originalAutoExposure = true;
  s.settings.captureExposure = 3.f;
  s.post.blend = true;
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  CHECK(run.state() == CalibrationRun::State::Opening);
  bool perProjector = false;
  for(int i = 0; i < 40000 && run.running(); i++)
  {
    rig.tick();
    if(run.state() == CalibrationRun::State::Capturing && rig.shown.size() == 2
       && run.session().capturing())
    {
      const int p = run.session().projector();
      const auto& mine = rig.shown[p];
      const auto& other = rig.shown[1 - p];
      perProjector |= mine && other && mine->rgba == run.session().image(p).rgba
                      && other->rgba == run.session().image(1 - p).rgba
                      && mine->rgba != other->rgba;
    }
  }
  const QString log = run.takeLog().join('\n');
  INFO(log.toStdString() << "\n" << run.status().toStdString());
  REQUIRE(run.state() == CalibrationRun::State::Done);
  CHECK(perProjector);
  CHECK(log.contains("Camera: 480x200"));
  for(const QString& n : {dir.filePath("hall"), dir.filePath("hall_p1")})
  {
    const auto c = loadCalibration(n + ".json");
    REQUIRE(c.ok());
    CHECK_FALSE(c.screen);
    CHECK(QFile::exists(n + ".points.pfm"));
  }
  CHECK(log.contains("Projector Blend Maps"));
  REQUIRE(!rig.exposures.empty());
  CHECK(rig.exposures.front() == 3.f);
  CHECK(rig.exposures.back() == 7.f);
  REQUIRE(!rig.autoExposures.empty());
  CHECK(rig.autoExposures.front() == false);
  CHECK(rig.autoExposures.back() == true);
  CHECK(rig.streaming == std::vector<bool>{true});
  CHECK(rig.clears >= 1);
  CHECK(run.progress() == 1.f);
}

TEST_CASE("CalibrationRun restores the camera when aborted", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  auto s = rig.setup(dir.filePath("hall"));
  s.originalExposure = 7.f;
  s.originalAutoExposure = true;
  s.settings.captureExposure = 3.f;
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  for(int i = 0; i < 2000 && !run.session().capturing(); i++)
    rig.tick();
  REQUIRE(run.session().capturing());
  rig.tick(20);
  run.abort("stop");
  CHECK(run.state() == CalibrationRun::State::Failed);
  CHECK(run.status() == "stop");
  REQUIRE(!rig.exposures.empty());
  CHECK(rig.exposures.back() == 7.f);
  REQUIRE(!rig.autoExposures.empty());
  CHECK(rig.autoExposures.back() == true);
  CHECK(rig.clears >= 1);
  CHECK_FALSE(QFile::exists(dir.filePath("hall.json")));
}

TEST_CASE("CalibrationRun aborted while computing writes no maps", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  auto s = rig.setup(dir.filePath("hall"));
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  rig.hold = [&] { return run.state() == CalibrationRun::State::Blending; };
  for(int i = 0; i < 40000 && run.state() != CalibrationRun::State::Blending && run.running(); i++)
    rig.tick();
  REQUIRE(run.state() == CalibrationRun::State::Blending);
  run.tick(rig.now);
  REQUIRE(rig.jobs.size() == 1);
  run.abort("closed");
  CHECK(run.state() == CalibrationRun::State::Failed);
  rig.jobs[0]();
  CHECK_FALSE(QFile::exists(dir.filePath("hall.points.pfm")));
  CHECK_FALSE(run.outcome());
}

TEST_CASE("CalibrationRun fails on a frozen camera instead of hanging", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  auto s = rig.setup(dir.filePath("frozen"));
  s.settings.timeoutMs = 500;
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  for(int i = 0; i < 2000 && !run.session().capturing(); i++)
    rig.tick();
  rig.bench.freezeAt = rig.now;
  rig.bench.noise = 0.f;
  rig.tick(20000);
  INFO(run.takeLog().join('\n').toStdString());
  CHECK(run.state() == CalibrationRun::State::Failed);
  CHECK(rig.now < 60000.);
}

TEST_CASE("CalibrationRun reopens a camera of the wrong size, then fails", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  rig.frameSize = {CW / 2, CH / 2};
  auto s = rig.setup(dir.filePath("small"));
  s.expectedCameraSize = {CW, CH};
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  rig.tick(2000);
  const QString log = run.takeLog().join('\n');
  INFO(log.toStdString());
  CHECK(run.state() == CalibrationRun::State::Failed);
  CHECK(run.status().contains("did not open in its mode"));
  CHECK(rig.streaming == std::vector<bool>{true, false, true, false, true});
  CHECK(log.count("reopening it in") == 2);
  CHECK_FALSE(run.session().running());
}

TEST_CASE("CalibrationRun follows a camera device created again", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  auto s = rig.setup(dir.filePath("hall"));
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  for(int i = 0; i < 2000 && !run.session().capturing(); i++)
    rig.tick();
  REQUIRE(run.session().capturing());
  rig.producer = Gfx::port_index{9, 0};
  rig.tick(3);
  REQUIRE(rig.producers.size() == 1);
  CHECK(rig.producers[0].node == 9);
  CHECK(rig.streaming == std::vector<bool>{true, true});

  rig.cameraState = Gfx::TextureCapture::State::Unavailable;
  rig.tick();
  CHECK(run.state() == CalibrationRun::State::Failed);
  CHECK(run.status().contains("unplugged"));

  CalibrationRun gone{rig.setup(dir.filePath("hall"))};
  rig.run = &gone;
  rig.windows = -1;
  CHECK_FALSE(gone.start(rig.now));
  CHECK(gone.status().contains("not running"));
}

TEST_CASE("CalibrationRun refuses outputs of different sizes", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  auto s = rig.setup(dir.filePath("hall"));
  s.plan.error = "different sizes";
  CalibrationRun run{std::move(s)};
  CHECK_FALSE(run.start(0.));
  CHECK(run.status() == "different sizes");
  CHECK(rig.streaming.empty());
}

TEST_CASE("CalibrationRun of some outputs leaves the others showing their content", "[threedim][calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  Rig rig;
  rig.windows = 3;
  auto s = rig.setup(dir.filePath("hall"));
  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  for(int i = 0; i < 3; i++)
  {
    Gfx::OutputMapping m;
    m.windowSize = {PW, PH};
    ws.outputs.push_back(m);
  }
  s.plan = planCalibration(ws, dir.filePath("hall"), std::vector<int>{1, 2}, 2, 4);
  CalibrationRun run{std::move(s)};
  rig.run = &run;
  REQUIRE(run.start(rig.now));
  bool window0Lit = false;
  for(int i = 0; i < 40000 && run.running(); i++)
  {
    rig.tick();
    if(rig.shown.size() == 3)
      window0Lit |= bool(rig.shown[0]);
  }
  INFO(run.takeLog().join('\n').toStdString());
  REQUIRE(run.state() == CalibrationRun::State::Done);
  CHECK_FALSE(window0Lit);
  CHECK_FALSE(QFile::exists(dir.filePath("hall.json")));
  CHECK(QFile::exists(dir.filePath("hall_p1.json")));
  CHECK(QFile::exists(dir.filePath("hall_p2.json")));
}
