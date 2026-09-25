// Consumers follow an upstream auxiliary whose size changes at run time
// (agent A5, aux-size siblings of 0e89159ade).
//
// An upstream publishes an `items` auxiliary of `count` vec4; after a few
// frames the count goes from 24 to 40. Two upstreams: csf-a5-aux-producer.cs,
// which reallocates the buffer, and a5::AuxProducerNode, which publishes a new
// range of the same buffer, like ScenePreprocessor. The raw raster kept the
// size it adopted with the buffer, and the CSF the size of a top-level storage
// resource, so both kept counting 24 against the same buffer. Each consumer
// reports what it sees as colour, and must report 40 after the change:
// - a MANUAL raw raster counting $COUNT_items invocations (red = count);
// - a raw raster whose output is $COUNT_items texels wide (red at the right
//   edge = width - 1);
// - a CSF reading `items` as a nested auxiliary and owning `copy`, sized
//   $COUNT_items (red = items.length(), green = copy.length());
// - the same with `items` name-matched into a top-level storage resource.
//
// N54 remainder: a producer publishing its auxiliary at an offset that is not
// a multiple of the 256-byte storage offset alignment gets its whole buffer
// bound from byte 0, as before, and now one warning saying so; an aligned
// offset binds the published range without a warning.
//
// Registration:
//   score_add_gfx_test(aux_size_a5 GfxAuxSizeA5.cpp)
#include <score_test/Gfx.hpp>

#include "A5AuxProducer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QMutex>
#include <QStringList>

#include <array>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE("consumers follow an upstream auxiliary resized at run time", "[gfx][auxiliary]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sameBuffer = GENERATE(false, true);
  const std::string consumer
      = GENERATE("rr-aux-count", "rr-a5-aux-width", "csf-a5-aux-consumer.cs",
                 "csf-a5-aux-toplevel.cs");
  CAPTURE(backend_name(api), sameBuffer, consumer);

  const bool raster = consumer.starts_with("rr-");
  const bool width = consumer == "rr-a5-aux-width";
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> before{}, after{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    a5::AuxProducerNode* node{};
    int producer = -1;
    if(sameBuffer)
    {
      auto n = std::make_unique<a5::AuxProducerNode>();
      node = n.get();
      producer = p.addNode(std::move(n));
    }
    else
    {
      producer = p.addCsf(corpus("csf-a5-aux-producer.cs"));
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
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("error=" << err);
  REQUIRE(err.empty());
  INFO("before " << int(before[0]) << " " << int(before[1]));
  INFO("after " << int(after[0]) << " " << int(after[1]));
  if(width)
  {
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

namespace
{
QMutex g_logMutex;
QStringList g_log;
QtMessageHandler g_prevHandler{};

void captureHandler(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
{
  {
    QMutexLocker lock{&g_logMutex};
    g_log.push_back(msg);
  }
  if(g_prevHandler)
    g_prevHandler(t, ctx, msg);
}
}

TEST_CASE(
    "a published offset off the storage alignment warns once and binds the whole buffer",
    "[gfx][auxiliary][n54]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int offset = GENERATE(16, 256);
  CAPTURE(backend_name(api), offset);

  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};
  {
    QMutexLocker lock{&g_logMutex};
    g_log.clear();
  }
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    g_prevHandler = qInstallMessageHandler(captureHandler);
    struct Restore
    {
      ~Restore() { qInstallMessageHandler(g_prevHandler); }
    } restore;
    GfxPipeline p;
    auto n = std::make_unique<a5::AuxProducerNode>();
    n->offset = offset;
    const int producer = p.addNode(std::move(n));
    const int cons = p.addCsf(corpus("csf-a5-aux-consumer.cs"));
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
  });
  if(skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("error=" << err);
  REQUIRE(err.empty());
  int warnings = 0;
  {
    QMutexLocker lock{&g_logMutex};
    for(const auto& m : g_log)
      if(m.contains(QStringLiteral("offset alignment")) && m.contains(QStringLiteral("a5.items")))
        warnings++;
  }
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
    "[gfx][auxiliary][n99]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sameBuffer = GENERATE(false, true);
  CAPTURE(backend_name(api), sameBuffer);

  bool skipped = false;
  std::string err;
  std::vector<std::array<uint8_t, 4>> frames;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int producer = -1;
    if(sameBuffer)
      producer = p.addNode(std::make_unique<a5::AuxProducerNode>());
    else
      producer = p.addCsf(corpus("csf-a5-aux-producer.cs"));
    const int cons = p.addCsf(corpus("csf-a5-aux-consumer.cs"));
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
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

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
