// The calibration wizard's plan for a multi-window device: projectors, sizes,
// names, selections; the calibration it assigns and its time estimates.

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
  CHECK(p.names == QStringList{"/cal/hall", "/cal/hall_p1"});
  CHECK(p.patternsPerProjector > 20);
  CHECK(p.warnings.isEmpty());

  ws.outputs[1].windowSize = {1280, 800};
  ws.outputs[1].rotation = 90;
  p = planCalibration(ws, "/cal/hall");
  CHECK(p.warnings.size() == 1);
  CHECK_FALSE(p.error.isEmpty());

  Gfx::WindowSettings single;
  CHECK(planCalibration(single, "/x").projectors == 0);
}

TEST_CASE("Calibration plan of six outputs runs them all", "[calibrationwizard]")
{
  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.inputWidth = 11520;
  ws.inputHeight = 1080;
  for(int i = 0; i < 6; i++)
    ws.outputs.push_back(output({i / 6., 0, 1. / 6., 1}, {1920, 1080}));
  const auto p = planCalibration(ws, "/cal/cave");
  CHECK(p.projectors == 6);
  CHECK(p.names.size() == 6);
  CHECK(p.names.back() == "/cal/cave_p5");
  INFO(p.warnings.join(" | ").toStdString());
  CHECK(p.warnings.isEmpty());
}

TEST_CASE("The wizard assigns the calibration and keeps the colour", "[calibrationwizard]")
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
  ws.outputs[1].color.gamma = 1.1f;

  ws.outputs[1].rotation = 90;
  ws.outputs[1].mirrorX = true;
  QStringList notes;
  const auto a = assignCalibrations(ws, {base, base + "_p1"}, 0, &notes);
  REQUIRE(notes.size() == 1);
  CHECK(notes[0].startsWith("output 1: edge blends, corner pin turned off"));
  CHECK(a.outputs[0].maps.calibration == base + ".json");
  CHECK(a.outputs[0].maps.alpha == base + ".alpha.pfm");
  CHECK(a.outputs[1].maps.alpha == base + "_p1.alpha.pfm");
  CHECK(a.outputs[0].color.linearBlend);
  CHECK(a.outputs[0].cornerWarp.isIdentity());
  CHECK(a.outputs[0].blendRight.width == 0.f);
  CHECK(a.outputs[1].color.gain == ws.outputs[1].color.gain);
  CHECK(a.outputs[1].color.gamma == 1.1f);
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

TEST_CASE("The wizard's post-processing estimate scales with the outputs", "[calibrationwizard]")
{
  CalibrationPlan p;
  p.projectors = 2;
  p.projectorSize = {3840, 2160};
  PostTiming t;
  CHECK(estimatePostMs(p, t) == 0.);
  t.blend = true;
  CHECK(estimatePostMs(p, t) == 2 * 1700.);
  t.blackCompensation = true;
  t.completePlanes = true;
  CHECK(estimatePostMs(p, t) == 2 * (12000. + 2000.));
  p.projectorSize = {1920, 1080};
  CHECK(estimatePostMs(p, t) == 2 * (12000. + 2000.) / 4.);
}

TEST_CASE("Outputs of different sizes are not calibrated in one run", "[calibrationwizard]")
{
  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.inputWidth = 3840;
  ws.inputHeight = 1080;
  ws.outputs = {output({0, 0, 0.5, 1}, {1920, 1080}), output({0.5, 0, 0.25, 1}, {1280, 720}),
                output({0.75, 0, 0.25, 1}, {1280, 720})};
  const auto all = planCalibration(ws, "/cal/hall");
  CHECK(all.error.contains("output 2 is 1280x720 and output 1 1920x1080"));
  const auto two = planCalibration(ws, "/cal/hall", std::vector<int>{1, 2}, 2, 4);
  CHECK(two.error.isEmpty());
  CHECK(two.projectorSize == QSize(1280, 720));
  const auto mixed = planCalibration(ws, "/cal/hall", std::vector<int>{1, 0}, 2, 4);
  CHECK(mixed.error.contains("output 1 is 1920x1080 and output 2 1280x720"));
}

TEST_CASE("Calibration of some outputs only", "[calibrationwizard]")
{
  CHECK(parseOutputSelection("", 3) == std::vector<int>{0, 1, 2});
  CHECK(parseOutputSelection("2-3", 6) == std::vector<int>{1, 2});
  CHECK(parseOutputSelection("5, 6 1", 6) == std::vector<int>{0, 4, 5});
  CHECK(parseOutputSelection("4-2;9", 6) == std::vector<int>{1, 2, 3});
  CHECK(parseOutputSelection("x", 2) == std::vector<int>{0, 1});
  CHECK(parseOutputSelection("1-2000000000", 3) == std::vector<int>{0, 1, 2});
  CHECK(parseOutputSelection("-2000000000-2", 3) == std::vector<int>{0, 1, 2});
  CHECK(parseOutputSelection("2-99999999999999", 4) == std::vector<int>{1, 2, 3});
  CHECK(parseOutputSelection("0-1", 4) == std::vector<int>{0});
  CHECK(parseOutputSelection("3", 0).empty());

  Gfx::WindowSettings ws;
  ws.mode = Gfx::WindowMode::MultiWindow;
  ws.inputWidth = 3 * 1920;
  ws.inputHeight = 1080;
  for(int i = 0; i < 3; i++)
    ws.outputs.push_back(output({i / 3., 0, 1. / 3., 1}, {1920, 1080}));
  const auto p = planCalibration(ws, "/cal/hall", std::vector<int>{1, 2}, 2, 4);
  CHECK(p.projectors == 2);
  CHECK(p.outputs == std::vector<int>{1, 2});
  CHECK(p.names == QStringList{"/cal/hall_p1", "/cal/hall_p2"});

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("hall");
  for(const QString& n : QStringList{base + "_p1", base + "_p2"})
    for(const char* ext : {".json", ".alpha.pfm"})
    {
      QFile f{n + ext};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(ext == std::string_view{".json"} ? "{}" : "x");
    }
  ws.outputs[0].maps.alpha = "/elsewhere/keep.alpha.pfm";
  ws.outputs[0].color.gain = {0.5f, 0.5f, 0.5f};
  const auto a = assignCalibrations(ws, {base + "_p1", base + "_p2"}, 0, {1, 2});
  CHECK(a.outputs[0].maps.alpha == "/elsewhere/keep.alpha.pfm");
  CHECK(a.outputs[0].color.gain[0] == 0.5f);
  CHECK(a.outputs[1].maps.alpha == base + "_p1.alpha.pfm");
  CHECK(a.outputs[2].maps.alpha == base + "_p2.alpha.pfm");
  CHECK(a.outputs[2].color.linearBlend);
}
