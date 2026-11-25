// Save Float Texture / Load Float Texture: the PFM files and the json
// sidecar of a calibration, through the objects themselves, with
// their worker run in place.

#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

using Threedim::LoadFloatTexture;
using Threedim::SaveFloatTexture;

namespace
{
template <typename T>
void inPlace(T& obj)
{
  obj.worker.request = [](auto jobs, uint64_t id, auto job) {
    T::worker::work(std::move(jobs), id, std::move(job));
  };
}

std::vector<float> rgbaPixels(int w, int h)
{
  std::vector<float> pixels(std::size_t(w) * h * 4);
  for(int i = 0; i < w * h; i++)
  {
    pixels[i * 4 + 0] = 0.1f * i;
    pixels[i * 4 + 1] = -1.5f + i;
    pixels[i * 4 + 2] = 2.f;
    pixels[i * 4 + 3] = (i % 2) ? 1.f : 0.f;
  }
  return pixels;
}

void pressSave(SaveFloatTexture& save)
{
  save.inputs.save.value = halp::impulse_type{};
  save();
  save.inputs.save.value.reset();
  save();
}

QByteArray readAll(const QString& path)
{
  QFile f{path};
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{};
}
}

TEST_CASE("Save Float Texture asks for its input once, on Save", "[threedim][floattexture]")
{
  QTemporaryDir dir;
  SaveFloatTexture save{};
  inPlace(save);
  save.inputs.file.value = dir.filePath("p.xyz.pfm").toStdString();
  save();
  CHECK_FALSE(save.inputs.image.request_readback);
  save.inputs.save.value = halp::impulse_type{};
  save();
  CHECK(save.inputs.image.request_readback);
  save.inputs.save.value.reset();
  for(int i = 0; i < SaveFloatTexture::readbackFrames + 1; i++)
    save();
  CHECK_FALSE(save.inputs.image.request_readback);
  CHECK(save.outputs.status.value == "Failed: nothing reached In");
  CHECK_FALSE(save.outputs.saved.value);

  save.inputs.file.value.clear();
  save.inputs.save.value = halp::impulse_type{};
  save();
  CHECK(save.outputs.status.value == "Failed: no file");
  CHECK_FALSE(save.inputs.image.request_readback);
}

TEST_CASE("Save then Load Float Texture round-trips an RGBA map", "[threedim][floattexture]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const int w = 5, h = 3;
  auto pixels = rgbaPixels(w, h);

  SaveFloatTexture save{};
  inPlace(save);
  save.inputs.image.texture.bytes = pixels.data();
  save.inputs.image.texture.width = w;
  save.inputs.image.texture.height = h;
  save.inputs.file.value = dir.filePath("proj0.XYZ.PFM").toStdString();
  save.inputs.sidecar.value = true;
  save.inputs.intrinsics.value = {1600.f, 1610.f, 959.5f, 539.5f};
  save.inputs.rms.value = 0.42f;
  pressSave(save);
  INFO(save.outputs.status.value);
  REQUIRE(save.outputs.saved.value);
  CHECK_FALSE(save.inputs.image.request_readback);
  CHECK(save.outputs.written.value == dir.filePath("proj0.xyz.pfm").toStdString());

  const auto o = Threedim::readJsonFile(dir.filePath("proj0.json"));
  CHECK(o["width"].toInt() == w);
  CHECK(o["height"].toInt() == h);
  CHECK(o["intrinsics"].toArray()[0].toDouble() == 1600.);
  CHECK(o["worldFromProjector"].toArray().size() == 16);
  CHECK(o["rms"].toDouble() == double(0.42f));
  CHECK(o["files"].toArray() == QJsonArray{"proj0.xyz.pfm", "proj0.valid.pfm"});

  const auto c = Threedim::loadCalibration(dir.filePath("proj0"));
  REQUIRE(c.ok());
  CHECK(c.xyz == pixels);
  CHECK(c.hasModel());

  LoadFloatTexture load{};
  inPlace(load);
  const QByteArray bytes = readAll(dir.filePath("proj0.xyz.pfm"));
  const std::string name = dir.filePath("proj0.xyz.pfm").toStdString();
  auto apply = LoadFloatTexture::ins::map_t::process(
      {.bytes = std::string_view(bytes.constData(), bytes.size()), .filename = name});
  apply(load);
  load();
  CHECK(load.outputs.loaded.value);
  CHECK(load.outputs.size.value.x == float(w));
  REQUIRE(load.outputs.texture.texture.bytes);
  CHECK(
      std::vector<float>(
          load.outputs.texture.texture.bytes, load.outputs.texture.texture.bytes + w * h * 4)
      == pixels);
}

TEST_CASE("Save Float Texture keeps the calibration it saves into", "[threedim][floattexture]")
{
  QTemporaryDir dir;
  const QString base = dir.filePath("p");
  const int w = 4, h = 2;
  {
    Threedim::CalibrationFile c;
    c.base = base;
    c.width = w;
    c.height = h;
    c.xyz = rgbaPixels(w, h);
    c.lum.assign(std::size_t(w) * h * 3, 0.5f);
    c.intrinsics = {900.f, 900.f, 1.5f, 0.5f};
    c.hasPose = true;
    c.pose[12] = 3.f;
    c.json["sequence"] = "gray";
    REQUIRE(Threedim::saveCalibration(c));
  }
  auto pixels = rgbaPixels(w, h);
  for(float& v : pixels)
    v += 1.f;

  SaveFloatTexture save{};
  inPlace(save);
  save.inputs.image.texture.bytes = pixels.data();
  save.inputs.image.texture.width = w;
  save.inputs.image.texture.height = h;
  save.inputs.file.value = QString(base + ".json").toStdString();

  SECTION("without the sidecar, a json listing the map is not written over")
  {
    pressSave(save);
    CHECK_FALSE(save.outputs.saved.value);
    CHECK(save.outputs.status.value.find("lists this map") != std::string::npos);
    CHECK(Threedim::loadCalibration(base).xyz == rgbaPixels(w, h));
  }
  SECTION("with the sidecar and no intrinsics, the model and the other keys stay")
  {
    save.inputs.sidecar.value = true;
    pressSave(save);
    INFO(save.outputs.status.value);
    REQUIRE(save.outputs.saved.value);
    const auto c = Threedim::loadCalibration(base);
    REQUIRE(c.ok());
    CHECK(c.xyz == pixels);
    CHECK(c.intrinsics[0] == 900.f);
    CHECK(c.pose[12] == 3.f);
    CHECK(c.lum.size() == std::size_t(w) * h * 3);
    CHECK(c.json["sequence"].toString() == "gray");
  }
  SECTION("a map of another size is refused")
  {
    save.inputs.sidecar.value = true;
    save.inputs.image.texture.width = 2;
    pressSave(save);
    CHECK_FALSE(save.outputs.saved.value);
    CHECK(Threedim::loadCalibration(base).xyz == rgbaPixels(w, h));
  }
  SECTION("a resampled input is refused")
  {
    save.inputs.sidecar.value = true;
    save.inputs.image.resampled = true;
    pressSave(save);
    CHECK_FALSE(save.outputs.saved.value);
    CHECK(save.outputs.status.value.find("RGBA32F") != std::string::npos);
  }
}

TEST_CASE("Save Float Texture writes RGB and the red channel alone", "[threedim][floattexture]")
{
  QTemporaryDir dir;
  std::vector<float> pixels{0.25f, 2.f, 3.f, 9.f, 0.75f, 4.f, 5.f, 9.f};
  SaveFloatTexture save{};
  inPlace(save);
  save.inputs.image.texture.bytes = pixels.data();
  save.inputs.image.texture.width = 2;
  save.inputs.image.texture.height = 1;

  save.inputs.file.value = dir.filePath("alpha").toStdString();
  save.inputs.channels.value = SaveFloatTexture::Channels::Red;
  pressSave(save);
  REQUIRE(save.outputs.saved.value);
  const auto grey = score::gfx::readPFM(dir.filePath("alpha.pfm"));
  REQUIRE(grey.valid());
  CHECK(grey.channels == 1);
  CHECK(grey.data == std::vector<float>{0.25f, 0.75f});
  CHECK_FALSE(QFile::exists(dir.filePath("alpha.json")));

  save.inputs.file.value = dir.filePath("rgb.pfm").toStdString();
  save.inputs.channels.value = SaveFloatTexture::Channels::RGB;
  pressSave(save);
  REQUIRE(save.outputs.saved.value);
  const auto rgb = score::gfx::readPFM(dir.filePath("rgb.pfm"));
  REQUIRE(rgb.channels == 3);
  CHECK(rgb.data == std::vector<float>{0.25f, 2.f, 3.f, 0.75f, 4.f, 5.f});
}

TEST_CASE("Load Float Texture reads the bytes it is given", "[threedim][floattexture]")
{
  SECTION("PFM rows are stored bottom to top")
  {
    QByteArray pfm = "Pf\n2 2\n-1.0\n";
    for(float v : {10.f, 11.f, 20.f, 21.f})
    {
      const quint32 le = qToLittleEndian(std::bit_cast<quint32>(v));
      pfm.append(reinterpret_cast<const char*>(&le), 4);
    }
    const auto l = LoadFloatTexture::load("/nonexistent/map.pfm", pfm);
    INFO(l.status);
    REQUIRE(l.image.valid());
    CHECK(l.image.row(0)[0] == 20.f);
    CHECK(l.image.row(0)[4] == 21.f);
    CHECK(l.image.row(1)[0] == 10.f);
    CHECK(l.image.row(1)[3] == 1.f);
  }
  SECTION("the bytes, not the file of that name")
  {
    QTemporaryDir dir;
    score::gfx::FloatImage onDisk;
    onDisk.width = onDisk.height = 1;
    onDisk.channels = 1;
    onDisk.data = {1.f};
    REQUIRE(score::gfx::writePFM(dir.filePath("m.pfm"), onDisk));
    auto given = onDisk;
    given.data = {7.f};
    const auto l = LoadFloatTexture::load(dir.filePath("m.pfm"), score::gfx::encodePFM(given));
    REQUIRE(l.image.valid());
    CHECK(l.image.data[0] == 7.f);
  }
  SECTION("a Bourke mesh")
  {
    const QByteArray mesh = "2\n2 2\n-1 -1 0 0 1\n 1 -1 1 0 1\n-1 1 0 1 1\n 1 1 1 1 1\n";
    const auto l = LoadFloatTexture::load("/nonexistent/w.data", mesh);
    INFO(l.status);
    CHECK(l.image.valid());
  }
  SECTION("not an image")
  {
    const auto l = LoadFloatTexture::load("/nonexistent/x.png", "garbage");
    CHECK_FALSE(l.image.valid());
    CHECK(l.status.starts_with("Failed"));
  }
}

TEST_CASE("Load Float Texture loads once for every renderer of its node", "[threedim][floattexture]")
{
  auto shared = std::make_shared<LoadFloatTexture::Jobs>();
  LoadFloatTexture a{}, b{};
  int requests = 0;
  for(auto* l : {&a, &b})
  {
    l->node_state = shared;
    l->worker.request = [&requests](auto jobs, uint64_t id, auto job) {
      requests++;
      LoadFloatTexture::worker::work(std::move(jobs), id, std::move(job));
    };
  }
  score::gfx::FloatImage img;
  img.width = 3;
  img.height = 1;
  img.channels = 1;
  img.data = {1.f, 2.f, 3.f};
  const QByteArray bytes = score::gfx::encodePFM(img);
  for(auto* l : {&a, &b})
    LoadFloatTexture::ins::map_t::process(
        {.bytes = std::string_view(bytes.constData(), bytes.size()), .filename = "m.pfm"})(*l);
  a();
  b();
  CHECK(requests == 1);
  CHECK(a.outputs.loaded.value);
  CHECK(b.outputs.loaded.value);
  REQUIRE(b.outputs.texture.texture.bytes);
  CHECK(b.outputs.texture.texture.bytes[8] == 3.f);
}
