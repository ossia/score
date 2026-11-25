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

TEST_CASE("A saved points map is the calibration's points map", "[threedim][calibration][points]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("floor");
  {
    QFile f{base + ".json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"({"files": ["floor.xyz.pfm"]})");
  }
  const std::vector<float> pts{1, 2, 3, 1, 4, 5, 6, 0};
  REQUIRE(savePointsMap(base, pts, 2, 1));
  Gfx::OutputMaps maps;
  Gfx::applyCalibration(maps, base + ".json");
  CHECK(maps.points == base + ".points.pfm");
  CHECK(Gfx::calibrationBase(base + ".points.valid.pfm") == base);
  const auto img = score::gfx::loadFloatImageRGBA(base + ".points.pfm");
  REQUIRE(img.width == 2);
  CHECK(img.data[0] == 1.f);
  CHECK(img.data[2] == 3.f);
  CHECK(img.data[3] == 1.f);
  CHECK(img.data[7] == 0.f);
}

namespace
{
std::vector<float> floorMap(int n, float y)
{
  std::vector<float> out;
  for(int j = 0; j < n; j++)
    for(int i = 0; i < n; i++)
    {
      const float x = -1.f + 2.f * i / (n - 1), z = -1.f + 2.f * j / (n - 1);
      out.insert(out.end(), {x, y, z, 1.f});
    }
  return out;
}
}

TEST_CASE("An equirect warp round a pole is refused", "[threedim][calibration][warp]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("floor");
  {
    QFile f{base + ".json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"({"files": []})");
  }
  const auto floor = equirectWarp(floorMap(9, -1.f), 9, 9);
  CHECK(floor.enclosesPole);
  QString err;
  CHECK_FALSE(saveEquirectWarp(base, floor, &err));
  CHECK(err.contains("pole"));
  CHECK_FALSE(QFile::exists(base + ".warp.pfm"));

  CHECK(equirectWarp(floorMap(9, 1.f), 9, 9).enclosesPole);

  auto wall = floorMap(9, -1.f);
  for(std::size_t i = 0; i < wall.size(); i += 4)
  {
    wall[i + 1] = wall[i + 2];
    wall[i + 2] = -2.f;
  }
  const auto w = equirectWarp(wall, 9, 9);
  CHECK_FALSE(w.enclosesPole);
  CHECK(saveEquirectWarp(base, w));
}

TEST_CASE("An equirect warp is rotated into the room", "[threedim][calibration][warp]")
{
  const std::array<double, 9> quarter{0, 0, 1, 0, 1, 0, -1, 0, 0};
  const auto w = equirectWarp(dirs({{1, 0, 0, 1}}), 1, 1, QRectF{0, 0, 1, 1}, &quarter);
  CHECK(w.uv[0] == Approx(0.5).margin(1e-6));
  CHECK(w.uv[1] == Approx(0.5).margin(1e-6));
}

TEST_CASE("A warp is not listed in a broken or missing calibration", "[threedim][calibration][warp]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto w = equirectWarp(dirs({{0, 0, -1, 1}}), 1, 1);
  QString err;
  CHECK_FALSE(saveEquirectWarp(dir.filePath("missing"), w, &err));
  CHECK(!err.isEmpty());

  const QString base = dir.filePath("broken");
  {
    QFile f{base + ".json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("{ \"files\": [");
  }
  err.clear();
  CHECK_FALSE(saveEquirectWarp(base, w, &err));
  CHECK(err.contains("JSON"));
  QFile f{base + ".json"};
  REQUIRE(f.open(QIODevice::ReadOnly));
  CHECK(f.readAll() == "{ \"files\": [");
}
