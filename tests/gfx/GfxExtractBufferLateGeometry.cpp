// Extract buffer reaches a GPU geometry whose buffers appear after its init.
//
// A compute shader's geometry output has no buffer yet when the downstream
// Extract buffer is initialised, and a CSF producer raises no dirty flag
// afterwards, so the node must resolve its input again in update(). Here
// guide-csf-geometry.cs feeds Extract buffer (Position), whose buffer is read
// by a raw raster painting its first element: vertex 0 is (-0.6, -0.5, 0.5, 1),
// so the frame must be (0, 0, 0.5); the unbound placeholder paints black.
#include "GfxHalpNodes.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/Document.hpp>

#include <Threedim/GeometryToBuffer.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
struct Shot
{
  bool skipped{};
  std::string error;
  std::array<uint8_t, 4> centre{};
};

Shot render(score::gfx::GraphicsApi api)
{
  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    const int producer = p.addCsf(corpus("guide-csf-geometry.cs"));
    auto extractNode = procs.make<Threedim::ExtractBuffer>(doc->context());
    auto* extract = extractNode.get();
    const int ex = p.addNode(std::move(extractNode));
    const int consumer
        = p.addRaster(corpus("rr-storage-input.vs"), corpus("rr-storage-input.fs"));
    if(producer < 0 || ex < 0 || consumer < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    auto* geoOut = p.geometryOut(producer, 0);
    auto* bufOut = p.nodeBufferOut(ex, 0);
    auto* bufIn = p.bufferIn(consumer, 0);
    if(!geoOut || !bufOut || !bufIn || extract->input.empty())
    {
      s.error = "ports missing";
      return;
    }
    p.wire(geoOut, extract->input[0]);
    p.wire(bufOut, bufIn);
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(6);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.error = "empty readback";
      return;
    }
    s.centre = img.at(img.width / 2, img.height / 2);
  });
  return s;
}
}

TEST_CASE(
    "Extract buffer outputs a CSF geometry's buffer created after its init",
    "[gfx][threedim][extractbuffer]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto s = render(api);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  INFO(
      "centre=(" << int(s.centre[0]) << "," << int(s.centre[1]) << ","
                 << int(s.centre[2]) << ")");
  CHECK(s.centre[0] < 20);
  CHECK(s.centre[1] < 20);
  CHECK(s.centre[2] > 100);
  CHECK(s.centre[2] < 160);
}
