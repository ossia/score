// Calibration map files: PFM as score stores it (rows bottom to top,
// a 4-channel map split into name.xyz.pfm + name.valid.pfm), Paul Bourke warp
// meshes rasterised to a UV map, and MPCDI archives.

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Gfx/Graph/WarpImport.hpp>

#include <score/tools/Zip.hpp>

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtEndian>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>

using namespace score::gfx;
using Catch::Approx;

namespace
{
FloatImage ramp(int w, int h, int channels)
{
  FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = channels;
  img.data.resize(std::size_t(w) * h * channels);
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
      for(int c = 0; c < channels; c++)
        img.row(y)[x * channels + c] = float(y * 1000 + x * 10 + c);
  return img;
}

void writeFile(const QString& path, const QByteArray& bytes)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(bytes);
}
}

TEST_CASE("PFM stores rows bottom to top, little-endian", "[gfx][calibration][pfm]")
{
  const auto img = ramp(3, 2, 3);
  const QByteArray bytes = encodePFM(img);
  REQUIRE(bytes.startsWith("PF\n3 2\n-1.0\n"));
  const char* data = bytes.constData() + bytes.indexOf("-1.0\n") + 5;
  float first;
  quint32 u;
  std::memcpy(&u, data, 4);
  u = qFromLittleEndian(u);
  std::memcpy(&first, &u, 4);
  CHECK(first == img.row(1)[0]);

  const auto back = parsePFM(bytes);
  REQUIRE(back.valid());
  CHECK(back.channels == 3);
  CHECK(back.data == img.data);

  QByteArray be = "Pf\n2 1\n1.0\n";
  for(float v : {0.25f, -2.f})
  {
    quint32 w;
    std::memcpy(&w, &v, 4);
    w = qToBigEndian(w);
    be.append(reinterpret_cast<const char*>(&w), 4);
  }
  const auto grey = parsePFM(be);
  REQUIRE(grey.valid());
  CHECK(grey.channels == 1);
  CHECK(grey.data == std::vector<float>{0.25f, -2.f});

  CHECK_FALSE(parsePFM("PF\n4 4\n-1.0\n0123").valid());
  CHECK_FALSE(parsePFM("P6\n4 4\n255\n").valid());
}

TEST_CASE("an RGBA map is written as xyz + valid and read back whole", "[gfx][calibration][pfm]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto img = ramp(4, 3, 4);
  const QString base = dir.filePath("out0");
  REQUIRE(writeFloatImageSplit(base, img));
  CHECK(QFile::exists(base + ".xyz.pfm"));
  CHECK(QFile::exists(base + ".valid.pfm"));
  CHECK(validMapPath(base + ".xyz.pfm") == base + ".valid.pfm");
  CHECK(validMapPath(dir.filePath("map.pfm")) == dir.filePath("map.valid.pfm"));

  const auto back = loadFloatImageRGBA(base + ".xyz.pfm");
  REQUIRE(back.valid());
  CHECK(back.channels == 4);
  CHECK(back.data == img.data);

  const auto alone = readPFM(base + ".xyz.pfm");
  REQUIRE(alone.valid());
  CHECK(alone.channels == 3);

  REQUIRE(writePFM(dir.filePath("grey.pfm"), ramp(2, 2, 1)));
  const auto grey = loadFloatImageRGBA(dir.filePath("grey.pfm"));
  REQUIRE(grey.valid());
  CHECK(grey.row(1)[0] == 1000.f);
  CHECK(grey.row(1)[4] == 1010.f);
  CHECK(grey.row(1)[6] == 1010.f);
  CHECK(grey.row(1)[7] == 1.f);
}

TEST_CASE("a PFM is replaced as a whole, never rewritten in place", "[gfx][calibration][pfm]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.filePath("map.pfm");
  const auto first = ramp(3, 2, 3);
  REQUIRE(writePFM(path, first));
  const QByteArray before = encodePFM(first);

  QFile reader{path};
  REQUIRE(reader.open(QIODevice::ReadOnly));

  auto second = ramp(5, 4, 1);
  REQUIRE(writePFM(path, second));
  CHECK(reader.readAll() == before);
  CHECK(readPFM(path).width == 5);

  FloatImage bad = ramp(2, 2, 4);
  CHECK_FALSE(writePFM(path, bad));
  CHECK(readPFM(path).width == 5);
}

TEST_CASE(
    "the projector response and compensation files are replaced as a whole",
    "[gfx][calibration][photometric]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("p");
  score::gfx::ProjectorResponseFile r;
  r.inverseSize = 8;
  REQUIRE(score::gfx::writeProjectorResponse(base, r));
  QFile reader{base + ".response.json"};
  REQUIRE(reader.open(QIODevice::ReadOnly));
  QByteArray first;
  {
    QFile f{base + ".response.json"};
    REQUIRE(f.open(QIODevice::ReadOnly));
    first = f.readAll();
  }
  r.inverseSize = 16;
  REQUIRE(score::gfx::writeProjectorResponse(base, r));
  CHECK(reader.readAll() == first);
  CHECK(score::gfx::loadResponseTable(base + ".response.json").size == 16);

  score::gfx::CompensationMaps comp;
  comp.width = 2;
  comp.height = 1;
  for(auto& row : comp.rows)
  {
    row.width = 2;
    row.height = 1;
    row.channels = 4;
    row.data.assign(8, 0.5f);
  }
  const auto files = score::gfx::encodeCompensation("p", comp, {});
  REQUIRE(files.maps.size() == 4);
  CHECK(files.maps[0].first == ".comp0.pfm");
  CHECK(files.json.suffix == ".comp.json");
  REQUIRE(score::gfx::writeCompensation(base, comp, {}));
  QFile compReader{base + ".comp.json"};
  REQUIRE(compReader.open(QIODevice::ReadOnly));
  const QByteArray compFirst = compReader.peek(1 << 20);
  QFile compHandle{base + ".comp.json"};
  REQUIRE(compHandle.open(QIODevice::ReadOnly));
  REQUIRE(score::gfx::writeCompensation(base, comp, {.scale = 2.}));
  CHECK(compHandle.readAll() == compFirst);
  CHECK(score::gfx::loadCompensation(base + ".comp.json").valid());
}

TEST_CASE("a 16-bit PNG loads as normalised floats", "[gfx][calibration]")
{
  QTemporaryDir dir;
  QImage img(2, 1, QImage::Format_Grayscale16);
  reinterpret_cast<quint16*>(img.scanLine(0))[0] = 0;
  reinterpret_cast<quint16*>(img.scanLine(0))[1] = 32768;
  REQUIRE(img.save(dir.filePath("a.png")));
  const auto f = loadFloatImageRGBA(dir.filePath("a.png"));
  REQUIRE(f.valid());
  CHECK(f.data[0] == Approx(0.f));
  CHECK(f.data[4] == Approx(32768.f / 65535.f).margin(1e-5));
}

TEST_CASE("a Bourke warp mesh rasterises to a UV map and intensity", "[gfx][calibration][bourke]")
{
  // 2x2 mesh covering the whole 2:1 frame, u flipped horizontally, v up,
  // intensity 1 on the left and 0.5 on the right.
  const QByteArray mesh = "2\n2 2\n"
                          "-2 -1 1 0 1\n"
                          " 2 -1 0 0 0.5\n"
                          "-2  1 1 1 1\n"
                          " 2  1 0 1 0.5\n";
  QString err;
  const auto w = importBourkeMesh(mesh, 0, 0, &err);
  INFO(err.toStdString());
  REQUIRE(w.map.valid());
  CHECK(w.width == 1024);
  CHECK(w.height == 512);

  const float* tl = w.map.row(0);
  CHECK(tl[0] == Approx(1.f - 0.5f / 1024.f).margin(1e-4));
  CHECK(tl[1] == Approx(0.5f / 512.f).margin(1e-4));
  CHECK(tl[3] == 1.f);
  const float* br = w.map.row(511) + 1023 * 4;
  CHECK(br[0] == Approx(0.5f / 1024.f).margin(1e-4));
  CHECK(br[1] == Approx(1.f - 0.5f / 512.f).margin(1e-4));
  CHECK(w.alpha.row(10)[0] == Approx(1.f).margin(1e-3));
  CHECK(w.alpha.row(10)[1023 * 4] == Approx(0.5f).margin(1e-3));

  const QByteArray dropped = "2\n3 2\n"
                             "-1 -1 0 0 -1\n"
                             " 0 -1 0.5 0 1\n"
                             " 1 -1 1 0 1\n"
                             "-1  1 0 1 1\n"
                             " 0  1 0.5 1 1\n"
                             " 1  1 1 1 1\n";
  const auto d = importBourkeMesh(dropped, 64, 64);
  REQUIRE(d.map.valid());
  CHECK(d.map.row(63)[3] == 0.f);
  CHECK(d.map.row(0)[10 * 4 + 3] == 0.f);
  CHECK(d.map.row(0)[63 * 4 + 3] == 1.f);
  CHECK(d.map.row(32)[48 * 4 + 0] == Approx(48.5f / 64.f).margin(1e-4));

  CHECK_FALSE(importBourkeMesh("1\n2 2\n", 8, 8).map.valid());
}

TEST_CASE("MPCDI regions load their warp, alpha and beta", "[gfx][calibration][mpcdi]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  FloatImage warp;
  warp.width = 4;
  warp.height = 2;
  warp.channels = 3;
  warp.data.resize(4 * 2 * 3);
  for(int y = 0; y < 2; y++)
    for(int x = 0; x < 4; x++)
    {
      float* p = warp.row(y) + x * 3;
      p[0] = x / 3.f;
      p[1] = y == 0 ? 0.75f : 0.25f;
      p[2] = 0.f;
    }
  warp.row(1)[3 * 3] = std::numeric_limits<float>::quiet_NaN();
  writeFile(dir.filePath("warp.pfm"), encodePFM(warp));

  QImage alpha(4, 2, QImage::Format_Grayscale8);
  alpha.fill(128);
  REQUIRE(alpha.save(dir.filePath("alpha.png")));
  QImage beta(4, 2, QImage::Format_Grayscale8);
  beta.fill(51);
  REQUIRE(beta.save(dir.filePath("beta.png")));

  writeFile(
      dir.filePath("mpcdi.xml"),
      R"(<?xml version="1.0" encoding="utf-8"?>
<MPCDI profile="2d" level="1" version="2.0">
  <display>
    <buffer id="buf" xResolution="1920" yResolution="1080">
      <region id="proj1" x="0" y="0" xsize="1" ysize="1" xResolution="1920" yResolution="1080"/>
    </buffer>
  </display>
  <files>
    <fileset region="proj1">
      <geometryWarpFile><path>warp.pfm</path><interpolation>linear</interpolation></geometryWarpFile>
      <alphaMap><path>alpha.png</path><componentDepth>1</componentDepth><bitDepth>8</bitDepth><gammaEmbedded>2.0</gammaEmbedded></alphaMap>
      <betaMap><path>beta.png</path><componentDepth>1</componentDepth><bitDepth>8</bitDepth></betaMap>
    </fileset>
  </files>
</MPCDI>)");

  const QString archive = dir.filePath("setup.mpcdi");
  QString err;
  REQUIRE(score::writeZipArchive(
      archive,
      {{dir.filePath("mpcdi.xml"), "mpcdi.xml"},
       {dir.filePath("warp.pfm"), "warp.pfm"},
       {dir.filePath("alpha.png"), "alpha.png"},
       {dir.filePath("beta.png"), "beta.png"}},
      0, err));

  const auto regions = loadMPCDI(archive, &err);
  INFO(err.toStdString());
  REQUIRE(regions.size() == 1);
  const auto& r = regions[0];
  CHECK(r.region == QStringLiteral("proj1"));
  CHECK_FALSE(r.threeD);
  CHECK(r.width == 1920);
  REQUIRE(r.map.valid());
  CHECK(r.map.row(0)[4 + 0] == Approx(1.f / 3.f));
  CHECK(r.map.row(0)[1] == Approx(0.25f));
  CHECK(r.map.row(1)[1] == Approx(0.75f));
  CHECK(r.map.row(1)[3 * 4 + 3] == 0.f);
  CHECK(r.map.row(1)[2 * 4 + 3] == 1.f);
  REQUIRE(r.alpha.valid());
  CHECK(r.alpha.data[0] == Approx(std::pow(128.f / 255.f, 2.f)).margin(1e-3));
  REQUIRE(r.beta.valid());
  CHECK(r.beta.data[0] == Approx(0.2f).margin(1e-3));

  const auto viaSpec = loadWarpMap(archive + "#proj1", WarpMapKind::Beta);
  REQUIRE(viaSpec.valid());
  CHECK(viaSpec.data[0] == Approx(0.2f).margin(1e-3));
  CHECK_FALSE(loadWarpMap(archive + "#nope", WarpMapKind::Map).valid());
}
