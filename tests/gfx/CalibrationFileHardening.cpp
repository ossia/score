// Crafted calibration files: truncated or CRLF PFM headers, Bourke meshes
// whose sizes overflow or whose raster would not fit in memory, MPCDI XML
// with nested filesets, and 1D .cube / response.json tables with bad ranges.

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Gfx/Graph/WarpImport.hpp>

#include <score/tools/Zip.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

using namespace score::gfx;

namespace
{
void writeFile(const QString& path, const QByteArray& bytes)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  REQUIRE(f.write(bytes) == bytes.size());
}

QByteArray littleFloat(float v)
{
  quint32 u;
  std::memcpy(&u, &v, 4);
  u = qToLittleEndian(u);
  return QByteArray(reinterpret_cast<const char*>(&u), 4);
}

double secondsOf(auto&& f)
{
  const auto t0 = std::chrono::steady_clock::now();
  f();
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}
}

TEST_CASE("a PFM whose scale ends the file is truncated", "[gfx][calibration][pfm]")
{
  QString err;
  CHECK_FALSE(parsePFM("PF\n1 1\n-1.0", &err).valid());
  CHECK_FALSE(err.isEmpty());
  CHECK_FALSE(parsePFM("Pf\n1 1\n-1.0").valid());
  CHECK_FALSE(parsePFM("Pf\n1 1\n-1.0\n").valid());
  CHECK_FALSE(parsePFM("Pf\n1 1\n-1.0\n123").valid());
  CHECK(parsePFM("Pf\n1 1\n-1.0\n" + littleFloat(2.f)).valid());
}

TEST_CASE("a CRLF PFM header loads unshifted", "[gfx][calibration][pfm]")
{
  const auto img = parsePFM("Pf\r\n2 1\r\n-1.0\r\n" + littleFloat(1.f) + littleFloat(-3.5f));
  REQUIRE(img.valid());
  CHECK(img.data == std::vector<float>{1.f, -3.5f});
  CHECK_FALSE(parsePFM("Pf\r\n2 1\r\n-1.0\r\n" + littleFloat(1.f)).valid());
}

TEST_CASE("only name.xyz.pfm pairs with name.valid.pfm", "[gfx][calibration][pfm]")
{
  CHECK(validMapPath("/a/room.v2.pfm") == "/a/room.v2.valid.pfm");
  CHECK(validMapPath("/a/room.v2.xyz.pfm") == "/a/room.v2.valid.pfm");
  CHECK(validMapPath("/a/room.xyz.pfm") == "/a/room.valid.pfm");
  CHECK(validMapPath("/a/room.pfm") == "/a/room.valid.pfm");
}

TEST_CASE("a Bourke mesh with overflowing sizes is rejected", "[gfx][calibration][bourke]")
{
  QString err;
  CHECK_FALSE(importBourkeMesh("2\n1500000000 1500000000\n", 0, 0, &err).map.valid());
  CHECK_FALSE(err.isEmpty());
  CHECK_FALSE(importBourkeMesh("2\n5000 2\n", 0, 0).map.valid());
  CHECK_FALSE(importBourkeMesh("2\n2 2\n0 0 0 0 1\n", 0, 0).map.valid());

  const QByteArray square = "2\n2 2\n-1 -1 0 0 1\n1 -1 1 0 1\n-1 1 0 1 1\n1 1 1 1 1\n";
  CHECK_FALSE(importBourkeMesh(square, 100000, 64).map.valid());
  CHECK_FALSE(importBourkeMesh(square, 16384, 16384).map.valid());
}

TEST_CASE("a Bourke mesh with a tiny aspect gets a bounded raster", "[gfx][calibration][bourke]")
{
  const QByteArray thin = "2\n2 2\n"
                          "-1e-30 -1 0 0 1\n"
                          " 1e-30 -1 1 0 1\n"
                          "-1e-30  1 0 1 1\n"
                          " 1e-30  1 1 1 1\n";
  const auto w = importBourkeMesh(thin, 0, 0);
  REQUIRE(w.map.valid());
  CHECK(w.width == 1024);
  CHECK(w.height == 4096);
}

TEST_CASE("a Bourke mesh of huge or non-finite nodes neither hangs nor crashes", "[gfx][calibration][bourke]")
{
  QByteArray huge = "2\n64 64\n";
  for(int j = 0; j < 64; j++)
    for(int i = 0; i < 64; i++)
      huge += QByteArray::number((i % 2) ? 1e5 : -1e5) + ' '
              + QByteArray::number((j % 2) ? 1e5 : -1e5) + " 0.5 0.5 1\n";
  QString err;
  const double t = secondsOf([&] { (void)importBourkeMesh(huge, 512, 512, &err); });
  CHECK(t < 5.);

  const QByteArray bad = "2\n2 2\n"
                         "nan -1 0 0 1\n"
                         "1e300 -1 1 0 1\n"
                         "-1 inf 0 1 1\n"
                         "1 1 1 1 nan\n";
  const auto w = importBourkeMesh(bad, 0, 0);
  CHECK(std::all_of(w.map.data.begin(), w.map.data.end(), [](float v) { return std::isfinite(v); }));
}

TEST_CASE("MPCDI paths after a nested fileset stay in bounds", "[gfx][calibration][mpcdi]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  FloatImage warp;
  warp.width = 2;
  warp.height = 1;
  warp.channels = 3;
  warp.data = {0.f, 0.f, 0.f, 1.f, 1.f, 0.f};
  writeFile(dir.filePath("warp.pfm"), encodePFM(warp));

  QByteArray xml = R"(<?xml version="1.0" encoding="utf-8"?>
<MPCDI profile="2d" level="1" version="2.0">
  <files>
    <fileset region="a">
      <geometryWarpFile>)";
  for(int k = 0; k < 64; k++)
    xml += "<fileset region=\"n" + QByteArray::number(k) + "\"/>";
  xml += R"(<path>warp.pfm</path><gammaEmbedded>2.0</gammaEmbedded>
        <originOf2DData>topLeft</originOf2DData>
      </geometryWarpFile>
      <alphaMap><path>warp.pfm</path></alphaMap>
    </fileset>
  </files>
</MPCDI>)";
  writeFile(dir.filePath("mpcdi.xml"), xml);

  const QString archive = dir.filePath("nested.mpcdi");
  QString err;
  REQUIRE(score::writeZipArchive(
      archive,
      {{dir.filePath("mpcdi.xml"), "mpcdi.xml"}, {dir.filePath("warp.pfm"), "warp.pfm"}}, 0,
      err));
  const auto regions = loadMPCDI(archive, &err);
  REQUIRE(regions.size() == 65);
  CHECK(regions[0].region == QStringLiteral("a"));
  CHECK_FALSE(regions[0].map.valid());
  CHECK_FALSE(regions[0].alpha.valid());
  for(std::size_t k = 1; k < regions.size(); k++)
    CHECK_FALSE(regions[k].map.valid());
}

TEST_CASE("1D .cube: LUT_1D_INPUT_RANGE, size bounds, non-finite entries", "[gfx][calibration][cube]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  auto load = [&](const QByteArray& text, QString* err = nullptr) {
    const QString p = dir.filePath("t.cube");
    writeFile(p, text);
    return loadResponseTable(p, err);
  };

  const auto ranged = load("LUT_1D_SIZE 2\nLUT_1D_INPUT_RANGE 0 2\n0 0 0\n1 1 1\n");
  REQUIRE(ranged.valid());
  CHECK(ranged.inverse[0] == Catch::Approx(0.f));
  CHECK(ranged.inverse[3] == Catch::Approx(0.5f));

  QString err;
  CHECK_FALSE(load("LUT_1D_SIZE 2\n0 0 0\nnan 1 1\n", &err).valid());
  CHECK_FALSE(err.isEmpty());
  CHECK_FALSE(load("LUT_1D_SIZE 2\n0 0 0\ninf 1 1\n").valid());
  CHECK_FALSE(load("LUT_1D_SIZE 2\nDOMAIN_MAX nan 1 1\n0 0 0\n1 1 1\n").valid());
  CHECK_FALSE(load("LUT_1D_SIZE 2\nDOMAIN_MIN 1 0 0\n0 0 0\n1 1 1\n").valid());
  CHECK_FALSE(load("LUT_1D_SIZE 100000000\n0 0 0\n1 1 1\n").valid());
  CHECK(load("LUT_1D_SIZE 2\n0 0 0\n1 1 1\n").valid());
}

TEST_CASE("response.json: a mixing grid beyond 17 levels or malformed is ignored", "[gfx][calibration][photometric]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  auto write = [&](int N, bool malformed) {
    QJsonObject o;
    QJsonArray inv;
    for(int c = 0; c < 3; c++)
      inv.append(QJsonArray{0., 1.});
    o["inverse"] = inv;
    QJsonObject mo;
    QJsonArray axes, shown;
    for(int c = 0; c < 3; c++)
    {
      QJsonArray a;
      for(int k = 0; k < N; k++)
        a.append(double(k) / (N - 1));
      axes.append(a);
    }
    for(int b = 0; b < N; b++)
      for(int g = 0; g < N; g++)
        for(int r = 0; r < N; r++)
          shown.append(
              malformed && r == 1 ? QJsonArray{0.}
                                  : QJsonArray{double(r) / (N - 1), double(g) / (N - 1),
                                               double(b) / (N - 1)});
    mo["axis"] = axes;
    mo["shown"] = shown;
    o["mixing"] = mo;
    const QString p = dir.filePath("m.response.json");
    writeFile(p, QJsonDocument(o).toJson());
    return p;
  };

  const auto ok = loadResponseTable(write(5, false));
  REQUIRE(ok.valid());
  CHECK(ok.mixing.valid());

  QString err;
  double t{};
  ResponseTable big;
  t = secondsOf([&] { big = loadResponseTable(write(30, false), &err); });
  REQUIRE(big.valid());
  CHECK_FALSE(big.mixing.valid());
  CHECK(t < 5.);

  const auto bad = loadResponseTable(write(3, true));
  REQUIRE(bad.valid());
  CHECK_FALSE(bad.mixing.valid());
}
