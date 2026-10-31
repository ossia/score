// The calibration wizard's plan for a multi-window device: projectors, canvas
// layout, names; the output stage it turns off and the calibration it assigns.

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationPlan.hpp>

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

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

  const auto a = assignCalibrations(ws, {base, base + "_p1"});
  CHECK(a.outputs[0].maps.calibration == base + ".json");
  CHECK(a.outputs[0].maps.alpha == base + ".alpha.pfm");
  CHECK(a.outputs[1].maps.alpha == base + "_p1.alpha.pfm");
  CHECK(a.outputs[0].color.linearBlend);
  CHECK_FALSE(a.outputs[0].cornerWarp.isIdentity());

  const auto r = readCalibrationResults({base, base + "_p1", base + "_p2"}, 0);
  CHECK(r.saved == 2);
  CHECK(r.rms[0] == 0.7);
  CHECK(r.coverage[1] == 88.5);
  CHECK(r.rms[2] < 0.);
  CHECK(readCalibrationResults({base}, QDateTime::currentMSecsSinceEpoch() + 60000).saved == 0);
}
