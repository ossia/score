#include <Gfx/Window/CalibrationSet.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/EquirectWarp.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using namespace Threedim;

namespace
{
std::vector<float> dirs(std::initializer_list<std::array<float, 4>> l)
{
  std::vector<float> out;
  for(const auto& d : l)
    out.insert(out.end(), d.begin(), d.end());
  return out;
}
}

TEST_CASE("Equirect warp follows the canvas convention", "[threedim][calibration][warp]")
{
  const auto w = equirectWarp(
      dirs({{0, 0, -2, 1}, {1, 0, 0, 1}, {-1, 0, 0, 1}, {0, 1, 0, 1}, {0, -1, -1e-6f, 1},
            {0, 0, -1, 0}}),
      6, 1);
  REQUIRE(w.uv.size() == 12);
  CHECK(w.uv[0] == Approx(0.5).margin(1e-6));
  CHECK(w.uv[1] == Approx(0.5).margin(1e-6));
  CHECK(w.uv[2] == Approx(0.75).margin(1e-6));
  CHECK(w.uv[4] == Approx(0.25).margin(1e-6));
  CHECK(w.uv[7] == Approx(0.).margin(1e-6));
  CHECK(w.uv[9] == Approx(1.).margin(1e-6));
  CHECK(w.valid == std::vector<float>{1, 1, 1, 1, 1, 0});
}

TEST_CASE("Equirect warp stays continuous across the seam", "[threedim][calibration][warp]")
{
  const float a = float(179. * M_PI / 180.);
  const auto w = equirectWarp(
      dirs({{std::sin(a), 0, -std::cos(a), 1}, {-std::sin(a), 0, -std::cos(a), 1}}), 2, 1);
  const double du = 2. / 360.;
  CHECK(std::abs(w.uv[0] - w.uv[2]) == Approx(du).margin(1e-5));
  CHECK(std::abs(w.uv[0] - 0.5) > 0.49);
}

TEST_CASE("Equirect warp is in the output's source rect", "[threedim][calibration][warp]")
{
  const auto w = equirectWarp(dirs({{1, 0, 0, 1}}), 1, 1, QRectF{0.5, 0., 0.5, 1.});
  CHECK(w.uv[0] == Approx(0.5).margin(1e-6));
  CHECK(w.uv[1] == Approx(0.5).margin(1e-6));
}

TEST_CASE("A saved equirect warp is the calibration's warp map", "[threedim][calibration][warp]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("wall");
  {
    QFile f{base + ".json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"({"files": ["wall.xyz.pfm", "wall.valid.pfm"]})");
  }
  const auto w = equirectWarp(dirs({{0, 0, -1, 1}, {1, 0, 0, 1}, {0, 1, 0, 0}, {0, 0, -1, 1}}), 2, 2);
  REQUIRE(saveEquirectWarp(base, w));
  CHECK(QFile::exists(base + ".warp.pfm"));
  CHECK(QFile::exists(base + ".warp.valid.pfm"));

  QFile f{base + ".json"};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto files = QJsonDocument::fromJson(f.readAll()).object().value("files").toArray();
  CHECK(files.contains(QStringLiteral("wall.warp.pfm")));
  CHECK(files.contains(QStringLiteral("wall.warp.valid.pfm")));
  CHECK(files.contains(QStringLiteral("wall.xyz.pfm")));

  Gfx::OutputMaps maps;
  Gfx::applyCalibration(maps, base + ".json");
  CHECK(maps.warp == base + ".warp.pfm");
  CHECK(Gfx::calibrationBase(base + ".warp.valid.pfm") == base);

  const auto img = score::gfx::loadFloatImageRGBA(base + ".warp.pfm");
  REQUIRE(img.width == 2);
  REQUIRE(img.height == 2);
  CHECK(img.data[0] == Approx(0.5).margin(1e-6));
  CHECK(img.data[4] == Approx(0.75).margin(1e-6));
  CHECK(img.data[3] == 1.f);
  CHECK(img.data[11] == 0.f);
}
