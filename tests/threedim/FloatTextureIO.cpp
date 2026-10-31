// Save Float Texture / Load Float Texture: the PFM files and json sidecar of
// immersive-analysis/CONVENTIONS.md, through the objects themselves.

#include <Threedim/FloatTextureIO.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using Threedim::LoadFloatTexture;
using Threedim::SaveFloatTexture;

TEST_CASE("Save Float Texture names its files", "[threedim][floattexture]")
{
  CHECK(SaveFloatTexture::basePath("/a/out0") == "/a/out0");
  CHECK(SaveFloatTexture::basePath("/a/out0.pfm") == "/a/out0");
  CHECK(SaveFloatTexture::basePath("/a/out0.xyz.pfm") == "/a/out0");
  CHECK(SaveFloatTexture::basePath("/a/out0.valid.pfm") == "/a/out0");
  CHECK(SaveFloatTexture::basePath("/a/out0.json") == "/a/out0");
}

TEST_CASE("Save then Load Float Texture round-trips an RGBA map", "[threedim][floattexture]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const int w = 5, h = 3;
  std::vector<float> pixels(w * h * 4);
  for(int i = 0; i < w * h; i++)
  {
    pixels[i * 4 + 0] = 0.1f * i;
    pixels[i * 4 + 1] = -1.5f + i;
    pixels[i * 4 + 2] = 2.f;
    pixels[i * 4 + 3] = (i % 2) ? 1.f : 0.f;
  }

  SaveFloatTexture save;
  save.inputs.image.texture.bytes = pixels.data();
  save.inputs.image.texture.width = w;
  save.inputs.image.texture.height = h;
  save.inputs.file.value = dir.filePath("proj0.xyz.pfm").toStdString();
  save.inputs.sidecar.value = true;
  save.inputs.intrinsics.value = {1600.f, 1610.f, 959.5f, 539.5f};
  save.inputs.rms.value = 0.42f;
  save();
  CHECK_FALSE(save.outputs.saved.value);

  CHECK_FALSE(save.inputs.image.request_readback);
  save.inputs.save.value = halp::impulse_type{};
  save();
  CHECK(save.inputs.image.request_readback);
  CHECK_FALSE(save.outputs.saved.value);
  save.inputs.save.value.reset();
  save();
  save();
  REQUIRE(save.outputs.saved.value);
  CHECK_FALSE(save.inputs.image.request_readback);
  CHECK(save.outputs.written.value == dir.filePath("proj0.xyz.pfm").toStdString());
  CHECK(QFile::exists(dir.filePath("proj0.valid.pfm")));

  QFile json{dir.filePath("proj0.json")};
  REQUIRE(json.open(QIODevice::ReadOnly));
  const auto o = QJsonDocument::fromJson(json.readAll()).object();
  CHECK(o["width"].toInt() == w);
  CHECK(o["height"].toInt() == h);
  CHECK(o["intrinsics"].toArray()[0].toDouble() == 1600.);
  CHECK(o["worldFromProjector"].toArray().size() == 16);
  CHECK(o["rms"].toDouble() == float(0.42f));

  const auto img = LoadFloatTexture::load(
      dir.filePath("proj0.xyz.pfm").toStdString(), {});
  REQUIRE(img.valid());
  CHECK(img.width == w);
  CHECK(img.height == h);
  CHECK(img.data == pixels);

  LoadFloatTexture load;
  auto apply = LoadFloatTexture::ins::map_t::process(
      {.bytes = {}, .filename = dir.filePath("proj0.xyz.pfm").toStdString()});
  apply(load);
  load();
  CHECK(load.outputs.loaded.value);
  CHECK(load.outputs.size.value.x == float(w));
  REQUIRE(load.outputs.texture.texture.bytes);
  CHECK(load.outputs.texture.texture.bytes[3 * 4 + 1] == pixels[3 * 4 + 1]);
}

TEST_CASE("Save Float Texture writes the red channel alone", "[threedim][floattexture]")
{
  QTemporaryDir dir;
  std::vector<float> pixels{0.25f, 9.f, 9.f, 9.f, 0.75f, 9.f, 9.f, 9.f};
  SaveFloatTexture save;
  save.inputs.image.texture.bytes = pixels.data();
  save.inputs.image.texture.width = 2;
  save.inputs.image.texture.height = 1;
  save.inputs.file.value = dir.filePath("alpha").toStdString();
  save.inputs.channels.value = SaveFloatTexture::Channels::Red;
  save.inputs.save.value = halp::impulse_type{};
  save();
  save.inputs.save.value.reset();
  save();
  save();
  REQUIRE(save.outputs.saved.value);
  const auto grey = score::gfx::readPFM(dir.filePath("alpha.pfm"));
  REQUIRE(grey.valid());
  CHECK(grey.channels == 1);
  CHECK(grey.data == std::vector<float>{0.25f, 0.75f});
}
