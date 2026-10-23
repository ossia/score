// A raw raster reads an upstream geometry auxiliary from the range it is
// published at. The auxiliary carries a byte_offset into its buffer (the
// ScenePreprocessor bucket shape, or a CSF republishing a borrowed range);
// name-matching it into a storage INPUT bound the buffer from byte 0, and a
// CPU-side auxiliary was uploaded from its first byte, whatever the offset
// (shader board U42, plan step 4). aux_range::AuxProducerNode publishes item i
// = (i + 1, 0, 0, 0) at `offset`: at 256 bytes, the first item read is 17.
// An offset the device cannot bind a buffer at (off its storage offset
// alignment, 4 here: no device has a smaller one) is refused like a buffer of
// the wrong kind or size: the block keeps its zero placeholder, with one
// warning. It was bound from byte 0, so the shader read the data that many
// bytes off. A range that moves at run time is followed.
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include "GfxAuxRangeProducer.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
// `items` read either as a nested AUXILIARY block or as a storage INPUT that
// the upstream auxiliary is name-matched into.
std::string fragment(bool nested)
{
  const std::string block = R"({ "NAME": "items", )"
                            + std::string(nested ? "" : R"("TYPE": "storage", )")
                            + R"("ACCESS": "read_only", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] })";
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "first" } ],
  )_" + std::string(nested ? R"("AUXILIARY": [ )" + block + " ],\n  \"INPUTS\": []"
                           : R"("INPUTS": [ )" + block + " ]")
         + R"_(,
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" }
}*/
void main() { first = vec4(items.data[0].x / 255.0, 0.0, 0.0, 1.0); }
)_";
}
}

namespace
{
struct Shot
{
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> before{}, after{};
  int refusals = 0;
};

// Renders with the producer at `offset`, then at `moveTo` when >= 0.
Shot shoot(score::gfx::GraphicsApi api, const QString& fs, int offset, int moveTo = -1)
{
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    auto n = std::make_unique<aux_range::AuxProducerNode>();
    n->offset = offset;
    auto* producerNode = n.get();
    const int producer = p.addNode(std::move(n));
    const int raster = p.addRaster(corpus("rr-aux-count.vs"), fs);
    if(producer < 0 || raster < 0)
    {
      s.err = "build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.err = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    auto img = p.readback(sink);
    if(!img.valid())
    {
      s.err = "readback failed";
      return;
    }
    s.before = img.center();
    if(moveTo >= 0)
    {
      producerNode->offset = moveTo;
      p.render(3);
      img = p.readback(sink);
      if(!img.valid())
      {
        s.err = "readback failed";
        return;
      }
      s.after = img.center();
    }
    s.refusals = log.count({u"items", u"cannot bind a buffer at"});
  });
  return s;
}

QString writeShader(const QTemporaryDir& dir, bool nested)
{
  const QString fs = dir.filePath(nested ? "aux-first-nested.fs" : "aux-first-input.fs");
  QFile f{fs};
  if(f.open(QIODevice::WriteOnly))
    f.write(QByteArray::fromStdString(fragment(nested)));
  return fs;
}
}

TEST_CASE(
    "a raw raster reads an auxiliary from its published offset", "[gfx][auxiliary][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool nested = GENERATE(true, false);
  const int offset = GENERATE(0, 256);
  CAPTURE(backend_name(api), nested, offset);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("aux-first.fs");
  {
    QFile f{fs};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QByteArray::fromStdString(fragment(nested)));
  }

  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto n = std::make_unique<aux_range::AuxProducerNode>();
    n->offset = offset;
    const int producer = p.addNode(std::move(n));
    const int raster = p.addRaster(corpus("rr-aux-count.vs"), fs);
    if(producer < 0 || raster < 0)
    {
      err = "build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    centre = img.center();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  INFO("first item read " << int(centre[0]));
  CHECK(int(centre[0]) == offset / 16 + 1);
}

TEST_CASE(
    "an auxiliary offset the device cannot bind at is refused, with one warning",
    "[gfx][auxiliary][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool nested = GENERATE(true, false);
  CAPTURE(backend_name(api), nested);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto s = shoot(api, writeShader(dir, nested), 4);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.err);
  REQUIRE(s.err.empty());
  CHECK(int(s.before[0]) == 0);
  CHECK(s.refusals == 1);
}

TEST_CASE(
    "a raw raster follows an auxiliary range that moves at run time",
    "[gfx][auxiliary][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool nested = GENERATE(true, false);
  CAPTURE(backend_name(api), nested);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto s = shoot(api, writeShader(dir, nested), 0, 512);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.err);
  REQUIRE(s.err.empty());
  CHECK(int(s.before[0]) == 1);
  CHECK(int(s.after[0]) == 512 / 16 + 1);
}
