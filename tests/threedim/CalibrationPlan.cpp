// The calibration wizard's plan for a multi-window device: projectors, canvas
// layout, names; the output stage it turns off and the calibration it assigns.

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationPlan.hpp>

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using namespace Threedim;

namespace
{
Gfx::OutputMapping output(QRectF src, QSize size)
{
  Gfx::OutputMapping m;
  m.sourceRect = src;
  m.windowSize = size;
  return m;
}
}

TEST_CASE("Calibration plan of a multi-window device", "[calibrationwizard]")
{
  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.inputWidth = 3840;
  ws.inputHeight = 1080;
  ws.outputs = {output({0, 0, 0.5, 1}, {1920, 1080}), output({0.5, 0, 0.5, 1}, {1920, 1080})};

  auto p = planCalibration(ws, "/cal/hall_p1.json");
  CHECK(p.projectors == 2);
  CHECK(p.projectorSize == QSize(1920, 1080));
  CHECK(p.canvasSize == QSize(3840, 1080));
  CHECK(p.canvasLayout == "0 0 0.5 1; 0.5 0 0.5 1");
  CHECK(p.names == QStringList{"/cal/hall", "/cal/hall_p1"});
  CHECK(p.patternsPerProjector > 20);
  CHECK(p.warnings.isEmpty());

  ws.outputs[1].windowSize = {1280, 800};
  ws.outputs[1].rotation = 90;
  p = planCalibration(ws, "/cal/hall");
  CHECK(p.warnings.size() == 3);

  Gfx::WindowSettings single;
  CHECK(planCalibration(single, "/x").projectors == 0);
}

TEST_CASE("The wizard bypasses the output stage and assigns the calibration", "[calibrationwizard]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("hall");
  for(const QString& n : QStringList{base, base + "_p1"})
  {
    QFile f{n + ".alpha.pfm"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    QFile j{n + ".json"};
    REQUIRE(j.open(QIODevice::WriteOnly));
    j.write(R"({"rms": 0.7, "coverage": 88.5})");
  }

  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.outputs = {output({0, 0, 0.5, 1}, {64, 64}), output({0.5, 0, 0.5, 1}, {64, 64})};
  ws.outputs[0].cornerWarp.topLeft = {0.1, 0.1};
  ws.outputs[0].blendRight.width = 0.2f;
  ws.outputs[0].maps.alpha = "/old.pfm";
  ws.outputs[1].color.gain = {0.5f, 0.5f, 0.5f};

  const auto n = neutralOutputs(ws);
  CHECK(n.outputs[0].cornerWarp.isIdentity());
  CHECK(n.outputs[0].blendRight.width == 0.f);
  CHECK(n.outputs[0].maps.empty());
  CHECK(n.outputs[1].color.isIdentity());
  CHECK(n.outputs[1].sourceRect == ws.outputs[1].sourceRect);

  ws.outputs[1].rotation = 90;
  ws.outputs[1].mirrorX = true;
  const auto a = assignCalibrations(ws, {base, base + "_p1"});
  CHECK(a.outputs[0].maps.calibration == base + ".json");
  CHECK(a.outputs[0].maps.alpha == base + ".alpha.pfm");
  CHECK(a.outputs[1].maps.alpha == base + "_p1.alpha.pfm");
  CHECK(a.outputs[0].color.linearBlend);
  CHECK(a.outputs[0].cornerWarp.isIdentity());
  CHECK(a.outputs[0].blendRight.width == 0.f);
  CHECK(a.outputs[1].color.gain == n.outputs[1].color.gain);
  CHECK(a.outputs[1].rotation == 90);
  CHECK(a.outputs[1].mirrorX);
  CHECK(a.outputs[1].sourceRect == ws.outputs[1].sourceRect);

  const auto stale
      = assignCalibrations(ws, {base, base + "_p1"}, QDateTime::currentMSecsSinceEpoch() + 60000);
  CHECK(stale.outputs[0].maps.calibration == base + ".json");
  CHECK(stale.outputs[0].maps.alpha.isEmpty());

  const auto r = readCalibrationResults({base, base + "_p1", base + "_p2"}, 0);
  CHECK(r.saved == 2);
  CHECK(r.rms[0] == 0.7);
  CHECK(r.coverage[1] == 88.5);
  CHECK(r.rms[2] < 0.);
  CHECK(readCalibrationResults({base}, QDateTime::currentMSecsSinceEpoch() + 60000).saved == 0);

  {
    QFile half{base + "_p1.json"};
    REQUIRE(half.open(QIODevice::WriteOnly | QIODevice::Truncate));
    half.write(R"({"rms": 0.7, "cove)");
  }
  const auto partial = readCalibrationResults({base, base + "_p1", base + "_p2"}, 0);
  CHECK(partial.saved == 1);
  REQUIRE(partial.rms.size() == 3);
  CHECK(partial.rms[1] < 0.);
  CHECK(partial.coverage[1] < 0.);
}

TEST_CASE("The wizard's capture estimate counts the photometric frames", "[calibrationwizard]")
{
  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.inputWidth = 3840;
  ws.inputHeight = 1080;
  ws.outputs = {output({0, 0, 0.5, 1}, {1920, 1080}), output({0.5, 0, 0.5, 1}, {1920, 1080})};
  const auto p = planCalibration(ws, "/cal/hall");

  CaptureTiming t;
  t.settleMs = 400.;
  t.framesToAverage = 2;
  t.cameraFrameMs = 50.;
  const double geometric = estimateCaptureMs(p, t);
  CHECK(geometric == 2 * p.patternsPerProjector * 500.);

  t.exposureSettleMs = 1500.;
  t.extraFrames = {160 + 24, 160};
  t.exposureChanges = {8, 0};
  const double photometric = estimateCaptureMs(p, t);
  CHECK(photometric == geometric + (160 + 24 + 160) * 500. + 8 * 1100.);
}

TEST_CASE("The wizard keeps a device's settings until the end of the run", "[calibrationwizard]")
{
  std::vector<std::pair<QString, Gfx::WindowSettings>> applied;
  bool fail = false;
  CalibrationSession s{[&](const QString& d, const Gfx::WindowSettings& w) {
    if(fail)
      return false;
    applied.emplace_back(d, w);
    return true;
  }};

  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.outputs = {output({0, 0, 1, 1}, {64, 64})};
  ws.outputs[0].cornerWarp.topLeft = {0.1, 0.1};
  ws.outputs[0].maps.warp = "/user.warp";

  REQUIRE(s.begin("proj", ws));
  CHECK(s.active());
  REQUIRE(applied.size() == 1);
  CHECK(applied[0].second.outputs[0].cornerWarp.isIdentity());

  CHECK_FALSE(s.begin("proj", applied[0].second));
  CHECK_FALSE(s.begin("other", ws));
  CHECK(applied.size() == 1);
  CHECK_FALSE(s.original()->outputs[0].cornerWarp.isIdentity());

  REQUIRE(s.restore());
  CHECK_FALSE(s.active());
  REQUIRE(applied.size() == 2);
  CHECK(applied[1].first == "proj");
  CHECK(applied[1].second.outputs[0].maps.warp == "/user.warp");
  CHECK_FALSE(applied[1].second.outputs[0].cornerWarp.isIdentity());

  REQUIRE(s.begin("proj", ws));
  REQUIRE(s.finish({"/cal/x"}, 0));
  CHECK_FALSE(s.active());
  CHECK(applied.back().second.outputs[0].maps.calibration == "/cal/x.json");
  CHECK(applied.back().second.outputs[0].maps.warp.isEmpty());
  CHECK(applied.back().second.outputs[0].cornerWarp.isIdentity());

  fail = true;
  CHECK_FALSE(s.begin("proj", ws));
  CHECK_FALSE(s.active());
  fail = false;
  REQUIRE(s.begin("proj", ws));
  s.drop();
  CHECK_FALSE(s.active());
  CHECK(s.restore());
}
