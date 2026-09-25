// Consumers follow an upstream auxiliary whose size changes at run time.
//
// An upstream publishes an `items` auxiliary of `count` vec4; after a few
// frames the count goes from 24 to 40. Two upstreams: csf-aux-count-producer.cs,
// which reallocates the buffer, and aux_range::AuxProducerNode, which publishes a new
// range of the same buffer, like ScenePreprocessor. Neither the size a raw
// raster adopted with the buffer nor the size of a CSF top-level storage
// resource may stay at 24 when the buffer is the same. Each consumer
// reports what it sees as colour, and must report 40 after the change:
// - a MANUAL raw raster counting $COUNT_items invocations (red = count);
// - a raw raster whose output is $COUNT_items texels wide (red at the right
//   edge = width - 1);
// - a CSF reading `items` as a nested auxiliary and owning `copy`, sized
//   $COUNT_items (red = items.length(), green = copy.length());
// - the same with `items` name-matched into a top-level storage resource.
//
// A producer publishing its auxiliary at an offset that is not a multiple of
// the 256-byte storage offset alignment gets its whole buffer bound from
// byte 0, with one warning saying so; an aligned
// offset binds the published range without a warning.
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include "GfxAuxRangeProducer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QStringList>

#include <array>
#include <string>
#include <vector>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

TEST_CASE("consumers follow an upstream auxiliary resized at run time", "[gfx][auxiliary]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sameBuffer = GENERATE(false, true);
  const std::string consumer
      = GENERATE("rr-aux-count", "rr-aux-width", "csf-aux-count-consumer.cs",
                 "csf-aux-count-toplevel.cs");
  CAPTURE(backend_name(api), sameBuffer, consumer);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  const bool raster = consumer.starts_with("rr-");
  const bool width = consumer == "rr-aux-width";
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> before{}, after{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    aux_range::AuxProducerNode* node{};
    int producer = -1;
    if(sameBuffer)
    {
      auto n = std::make_unique<aux_range::AuxProducerNode>();
      node = n.get();
      producer = p.addNode(std::move(n));
    }
    else
    {
      producer = p.addCsf(corpus("csf-aux-count-producer.cs"));
    }
    const int cons = raster ? p.addRaster(
                                  corpus((consumer + ".vs").c_str()),
                                  corpus((consumer + ".fs").c_str()))
                            : p.addCsf(corpus(consumer.c_str()));
    if(producer < 0 || cons < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(
        sameBuffer ? p.nodeGeometryOut(producer, 0) : p.geometryOut(producer, 0),
        p.geometryIn(cons, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    before = width ? img.at(img.width - 1, img.height / 2) : img.center();

    if(sameBuffer)
      node->count = 40;
    else
      setControl(*p.isf(producer), nth_control_input(*p.isf(producer), 0), ossia::value{40});
    p.render(4);
    img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    after = width ? img.at(img.width - 1, img.height / 2) : img.center();
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());
  INFO("before " << int(before[0]) << " " << int(before[1]));
  INFO("after " << int(after[0]) << " " << int(after[1]));
  if(width)
  {
    // The sink stretches the COUNT-texel output to 32 columns; its last column
    // samples the rightmost texel or, with linear filtering, blends in its
    // left neighbour.
    CHECK(before[0] >= 22);
    CHECK(before[0] <= 23);
    CHECK(after[0] >= 38);
    CHECK(after[0] <= 39);
  }
  else
  {
    CHECK(before[0] == 24);
    CHECK(after[0] == 40);
  }
  if(!raster)
  {
    CHECK(before[1] == 24);
    CHECK(after[1] == 40);
  }
}

TEST_CASE(
    "a published offset off the storage alignment warns once and binds the whole buffer",
    "[gfx][auxiliary][alignment]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int offset = GENERATE(16, 256);
  CAPTURE(backend_name(api), offset);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};
  int warnings = 0;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    auto n = std::make_unique<aux_range::AuxProducerNode>();
    n->offset = offset;
    const int producer = p.addNode(std::move(n));
    const int cons = p.addCsf(corpus("csf-aux-count-consumer.cs"));
    if(producer < 0 || cons < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(cons, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    centre = img.center();
    warnings = log.count({u"offset alignment", u"aux_range.items"});
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());
  INFO("centre " << int(centre[0]) << " " << int(centre[1]));
  if(offset == 16)
  {
    CHECK(warnings == 1);
    CHECK(centre[0] == 255);
  }
  else
  {
    CHECK(warnings == 0);
    CHECK(centre[0] == 24);
  }
}

TEST_CASE(
    "a SIZE read from an upstream auxiliary sizes the buffer before the first dispatch",
    "[gfx][auxiliary][size]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sameBuffer = GENERATE(false, true);
  CAPTURE(backend_name(api), sameBuffer);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  bool skipped = false;
  std::string err;
  std::vector<std::array<uint8_t, 4>> frames;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int producer = -1;
    if(sameBuffer)
      producer = p.addNode(std::make_unique<aux_range::AuxProducerNode>());
    else
      producer = p.addCsf(corpus("csf-aux-count-producer.cs"));
    const int cons = p.addCsf(corpus("csf-aux-count-consumer.cs"));
    if(producer < 0 || cons < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(
        sameBuffer ? p.nodeGeometryOut(producer, 0) : p.geometryOut(producer, 0),
        p.geometryIn(cons, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < 4; f++)
    {
      p.render(1);
      const auto img = p.readback(sink);
      if(!img.valid())
      {
        err = "readback failed";
        return;
      }
      frames.push_back(img.center());
    }
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());
  bool sawUpstream = false;
  for(std::size_t f = 0; f < frames.size(); f++)
  {
    INFO("frame " << f << ": items " << int(frames[f][0]) << " copy " << int(frames[f][1]));
    if(frames[f][0] == 24)
    {
      sawUpstream = true;
      CHECK(frames[f][1] == 24);
    }
  }
  CHECK(sawUpstream);
}
