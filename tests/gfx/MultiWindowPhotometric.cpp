// Photometric maps of the multi-window output: RGB alpha / beta maps (a grey
// map broadcasts, as before), the measured projector response replacing the
// 2.2 encode, and the radiometric compensation matrix. Read back from the
// presented swap chain of a real window; SKIPs without a windowing system.
//
// Content is tests/gfx/corpus/isf-gradient-x.fs: red = x, green = blue = 0.2.

#include "MultiWindowTestCommon.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>

using namespace score::test;
using namespace score::test::gfx;

using namespace score::test::gfx::multiwindow;

namespace
{
Gfx::OutputMapping undithered()
{
  auto m = single();
  m.quality.dither = Gfx::OutputDither::Off;
  return m;
}

void writeMap(const QString& path, int w, int h, std::vector<float> rgbOrGrey)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = int(rgbOrGrey.size());
  for(int i = 0; i < w * h; i++)
    img.data.insert(img.data.end(), rgbOrGrey.begin(), rgbOrGrey.end());
  REQUIRE(score::gfx::writePFM(path, img));
}

void writeResponse(const QString& path, double codeGamma, double mixGain = 0.)
{
  QJsonArray inv;
  for(int c = 0; c < 3; c++)
  {
    QJsonArray a;
    for(int i = 0; i < 256; i++)
      a.append(std::pow(i / 255., codeGamma));
    inv.append(a);
  }
  QJsonObject o;
  o["type"] = "projectorResponse";
  o["inverse"] = inv;
  if(mixGain > 0.)
  {
    QJsonArray shown;
    for(int b = 0; b < 2; b++)
      for(int g = 0; g < 2; g++)
        for(int r = 0; r < 2; r++)
          shown.append(QJsonArray{mixGain * r, mixGain * g, mixGain * b});
    o["mixing"] = QJsonObject{
        {"axis", QJsonArray{QJsonArray{0., 1.}, QJsonArray{0., 1.}, QJsonArray{0., 1.}}},
        {"shown", shown}};
  }
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(QJsonDocument(o).toJson());
}

double lin(double v)
{
  return std::pow(v, 2.2);
}
double enc(double v)
{
  return std::pow(std::clamp(v, 0., 1.), 1. / 2.2);
}
}

TEST_CASE("multi-window RGB alpha and beta maps; a grey map broadcasts", "[gfx][window][multiwindow][photometric]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeMap(dir.filePath("grey.alpha.pfm"), 32, 16, {0.5f});
  writeMap(dir.filePath("rgbgrey.alpha.pfm"), 32, 16, {0.5f, 0.5f, 0.5f});
  writeMap(dir.filePath("rgb.alpha.pfm"), 32, 16, {1.f, 0.5f, 0.25f});
  writeMap(dir.filePath("rgb.beta.pfm"), 8, 8, {0.f, 0.02f, 0.05f});

  auto m = undithered();
  m.color.linearBlend = true;
  m.maps.alpha = dir.filePath("grey.alpha.pfm");

  Frame grey, rgbGrey, rgb;
  bool ok[3]{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok[0] = grab(rig, grey);
    auto maps = m.maps;
    maps.alpha = dir.filePath("rgbgrey.alpha.pfm");
    rig.node->setMaps(0, maps);
    ok[1] = grab(rig, rgbGrey);
    maps.alpha = dir.filePath("rgb.alpha.pfm");
    maps.beta = dir.filePath("rgb.beta.pfm");
    rig.node->setMaps(0, maps);
    ok[2] = grab(rig, rgb);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok[0]);
  REQUIRE(ok[1]);
  REQUIRE(ok[2]);

  int maxDiff = 0;
  for(double x : {0.1, 0.3, 0.5, 0.7, 0.9})
    for(int c = 0; c < 3; c++)
      maxDiff = std::max(maxDiff, std::abs(grey.at(x, 0.5)[c] - rgbGrey.at(x, 0.5)[c]));
  INFO("grey Pf vs grey PF max difference " << maxDiff);
  CHECK(maxDiff == 0);
  CHECK(std::abs(grey.at(0.5, 0.5)[1] - u8(enc(lin(0.2) * 0.5))) <= 3);

  const auto p = rgb.at(0.75, 0.5);
  INFO("RGB alpha: " << p[0] << " " << p[1] << " " << p[2]);
  CHECK(std::abs(p[0] - u8(enc(lin(0.75) * 1.0))) <= 3);
  CHECK(std::abs(p[1] - u8(enc(lin(0.2) * 0.5 + 0.02))) <= 3);
  CHECK(std::abs(p[2] - u8(enc(lin(0.2) * 0.25 + 0.05))) <= 3);
}

TEST_CASE("multi-window measured response replaces the 2.2 encode", "[gfx][window][multiwindow][photometric]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeResponse(dir.filePath("p22.response.json"), 1. / 2.2);
  writeResponse(dir.filePath("p18.response.json"), 1. / 1.8);
  {
    QFile cube{dir.filePath("lin.cube")};
    REQUIRE(cube.open(QIODevice::WriteOnly));
    cube.write("LUT_1D_SIZE 2\n0 0 0\n1 1 1\n");
  }

  auto m = undithered();
  Frame none, p22, p18, linear;
  bool ok[4]{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok[0] = grab(rig, none);
    auto maps = m.maps;
    maps.response = dir.filePath("p22.response.json");
    rig.node->setMaps(0, maps);
    ok[1] = grab(rig, p22);
    maps.response = dir.filePath("p18.response.json");
    rig.node->setMaps(0, maps);
    ok[2] = grab(rig, p18);
    maps.response = dir.filePath("lin.cube");
    rig.node->setMaps(0, maps);
    ok[3] = grab(rig, linear);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  for(bool b : ok)
    REQUIRE(b);
  for(double x : {0.25, 0.5, 0.75})
  {
    const double r = (std::floor(x * 128) + 0.5) / 128.;
    INFO("x " << x);
    CHECK(std::abs(none.at(x, 0.5)[0] - u8(r)) <= 3);
    CHECK(std::abs(p22.at(x, 0.5)[0] - u8(r)) <= 3);
    CHECK(std::abs(p18.at(x, 0.5)[0] - u8(std::pow(lin(r), 1. / 1.8))) <= 3);
    CHECK(std::abs(linear.at(x, 0.5)[0] - u8(lin(r))) <= 3);
  }
}

TEST_CASE("multi-window radiometric compensation matrix", "[gfx][window][multiwindow][photometric]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  score::gfx::CompensationMaps comp;
  comp.width = 16;
  comp.height = 8;
  const float rows[3][4] = {{0.f, 1.f, 0.f, 0.f}, {1.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.5f, 0.1f}};
  for(int k = 0; k < 3; k++)
  {
    auto& r = comp.rows[k];
    r.width = 16;
    r.height = 8;
    r.channels = 4;
    for(int i = 0; i < 16 * 8; i++)
      r.data.insert(r.data.end(), rows[k], rows[k] + 4);
  }
  REQUIRE(score::gfx::writeCompensation(dir.filePath("swap"), comp, {}));
  const auto back = score::gfx::loadCompensation(dir.filePath("swap.comp.json"));
  REQUIRE(back.valid());
  CHECK(back.rows[2].data[3] == 0.1f);
  REQUIRE(QFile::copy(dir.filePath("swap.comp.json"), dir.filePath("swap.scaled.json")));
  const auto renamed = score::gfx::loadCompensation(dir.filePath("swap.scaled.json"));
  REQUIRE(renamed.valid());
  CHECK(renamed.rows[2].data[3] == 0.1f);

  auto m = undithered();
  m.maps.compensation = dir.filePath("swap.comp.json");
  Frame f;
  bool ok{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) { ok = grab(rig, f); });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  const double r = (std::floor(0.75 * 128) + 0.5) / 128.;
  const auto p = f.at(0.75, 0.5);
  INFO("compensated: " << p[0] << " " << p[1] << " " << p[2]);
  CHECK(std::abs(p[0] - u8(0.2)) <= 3);
  CHECK(std::abs(p[1] - u8(r)) <= 3);
  CHECK(std::abs(p[2] - u8(enc(0.5 * lin(0.2) + 0.1))) <= 3);
}

TEST_CASE("multi-window compensation keeps the hue of out-of-range colours", "[gfx][window][multiwindow][photometric]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  score::gfx::CompensationMaps comp;
  comp.width = 16;
  comp.height = 8;
  const float rows[3][4] = {{4.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}};
  for(int k = 0; k < 3; k++)
  {
    auto& r = comp.rows[k];
    r.width = 16;
    r.height = 8;
    r.channels = 4;
    for(int i = 0; i < 16 * 8; i++)
      r.data.insert(r.data.end(), rows[k], rows[k] + 4);
  }
  REQUIRE(score::gfx::writeCompensation(dir.filePath("gain"), comp, {}));

  auto m = undithered();
  m.maps.compensation = dir.filePath("gain.comp.json");
  Frame f;
  bool ok{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) { ok = grab(rig, f); });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  for(double x : {0.25, 0.75})
  {
    const double r = (std::floor(x * 128) + 0.5) / 128.;
    const double red = 4. * lin(r);
    const double k = red > 1. ? 1. / red : 1.;
    const auto p = f.at(x, 0.5);
    INFO("x " << x << ": " << p[0] << " " << p[1] << " " << p[2]);
    CHECK(std::abs(p[0] - u8(enc(red * k))) <= 3);
    CHECK(std::abs(p[1] - u8(enc(lin(0.2) * k))) <= 3);
    CHECK(std::abs(p[2] - u8(enc(lin(0.2) * k))) <= 3);
  }
}

TEST_CASE("multi-window compensation inverts the projector colour mixing", "[gfx][window][multiwindow][photometric]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeResponse(dir.filePath("half.response.json"), 1. / 2.2, 0.5);
  const auto table = score::gfx::loadResponseTable(dir.filePath("half.response.json"));
  REQUIRE(table.mixing.valid());

  score::gfx::CompensationMaps comp;
  comp.width = 16;
  comp.height = 8;
  for(int k = 0; k < 3; k++)
  {
    auto& r = comp.rows[k];
    r.width = 16;
    r.height = 8;
    r.channels = 4;
    for(int i = 0; i < 16 * 8; i++)
      for(int j = 0; j < 4; j++)
        r.data.push_back(j == k ? 1.f : 0.f);
  }
  REQUIRE(score::gfx::writeCompensation(dir.filePath("id"), comp, {}));

  auto m = undithered();
  m.maps.response = dir.filePath("half.response.json");
  m.maps.compensation = dir.filePath("id.comp.json");
  Frame f;
  bool ok{};
  int rowsWithComp{}, rowsWithout{}, rowsBack{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok = grab(rig, f);
    const auto& wo = rig.node->windowOutputs()[0];
    rowsWithComp = wo.responseMap ? wo.responseMap->height : 0;
    auto maps = m.maps;
    maps.compensation.clear();
    rig.node->setMaps(0, maps);
    rowsWithout = wo.responseMap ? wo.responseMap->height : 0;
    rig.node->setMaps(0, m.maps);
    rowsBack = wo.responseMap ? wo.responseMap->height : 0;
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  // The mixing table is only packed when a compensation map can use it.
  CHECK(rowsWithComp > 1);
  CHECK(rowsWithout == 1);
  CHECK(rowsBack == rowsWithComp);
  for(double x : {0.25, 0.75})
  {
    const double r = (std::floor(x * 128) + 0.5) / 128.;
    const double l[3] = {2. * lin(r), 2. * lin(0.2), 2. * lin(0.2)};
    const double k = std::max(1., l[0]);
    const auto p = f.at(x, 0.5);
    INFO("x " << x << ": " << p[0] << " " << p[1] << " " << p[2]);
    for(int c = 0; c < 3; c++)
      CHECK(std::abs(p[c] - u8(enc(l[c] / k))) <= 4);
  }
}

TEST_CASE("multi-window response of a large 1D cube is resampled", "[gfx][window][multiwindow][photometric]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const int N = 65536;
  {
    QByteArray cube = "LUT_1D_SIZE 65536\n";
    cube.reserve(N * 30);
    for(int i = 0; i < N; i++)
    {
      const QByteArray v = QByteArray::number(enc(double(i) / (N - 1)), 'f', 6);
      cube += v + ' ' + v + ' ' + v + '\n';
    }
    QFile f{dir.filePath("big.cube")};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(cube);
  }
  auto m = undithered();
  m.maps.response = dir.filePath("big.cube");
  Frame f;
  bool ok{};
  int width{}, height{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    const auto& wo = rig.node->windowOutputs()[0];
    width = wo.responseMap ? wo.responseMap->width : 0;
    height = wo.responseMap ? wo.responseMap->height : 0;
    ok = grab(rig, f);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  CHECK(width >= 2);
  CHECK(width <= 4096);
  CHECK(height == 1);
  const double r = (std::floor(0.75 * 128) + 0.5) / 128.;
  CHECK(std::abs(f.at(0.75, 0.5)[0] - u8(r)) <= 3);
}

TEST_CASE("projector response files load with their colour mixing", "[gfx][photometric]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeResponse(dir.filePath("plain.response.json"), 1. / 2.2);
  writeResponse(dir.filePath("mixed.response.json"), 1. / 2.2, 0.5);
  const auto plain = score::gfx::loadResponseTable(dir.filePath("plain.response.json"));
  const auto mixed = score::gfx::loadResponseTable(dir.filePath("mixed.response.json"));
  REQUIRE(plain.valid());
  REQUIRE(mixed.valid());
  CHECK(plain.size == 256);
  CHECK_FALSE(plain.mixing.valid());
  CHECK(mixed.mixing.valid());
  CHECK(plain.inverse[255 * 3] == Catch::Approx(1.f));
}
