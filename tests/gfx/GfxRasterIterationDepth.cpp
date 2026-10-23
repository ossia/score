// A raw raster iteration drew into a scratch depth per iteration, so the depth
// OUTPUT the shader declares, and publishes on its port, was never written
// when the colour target iterated (shader board R31). The iteration whose
// size and shape match the depth output attaches it: here level 0 of a
// PER_MIP target, the same size as the depth output, writes it as a single
// pass does.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>
#include <string>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
std::string fragment(const std::string& model)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "target", "TYPE": "color", "FORMAT": "rgba8", "WIDTH": 64, "HEIGHT": 64 },
    { "NAME": "depth", "TYPE": "depth", "FORMAT": "d32f", "WIDTH": 64, "HEIGHT": 64 }
  ],)_" + model + R"_(
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true, "DEPTH_WRITE": true, "DEPTH_COMPARE": "always",
    "CULL_MODE": "none", "TOPOLOGY": "triangles", "VERTEX_COUNT": 3
  }
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
}

struct Depth
{
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> covered{}, uncovered{};
};

Depth renderDepth(score::gfx::GraphicsApi api, const std::string& model)
{
  Depth d;
  QTemporaryDir dir;
  const QString fs = dir.filePath("iter-depth.fs");
  {
    QFile f{fs};
    if(!dir.isValid() || !f.open(QIODevice::WriteOnly))
    {
      d.err = "cannot write the shader";
      return d;
    }
    f.write(QByteArray::fromStdString(fragment(model)));
  }
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(corpus("rr-msaa-edge.vs"), fs);
    const int view = p.addIsf(corpus("msaa-input-view.fs"));
    if(raster < 0 || view < 0)
    {
      d.err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 1), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      d.skipped = p.skipped();
      d.err = d.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      d.err = "empty readback";
      return;
    }
    // The edge triangle covers the left of the frame, not the right corners.
    d.covered = img.at(6, 32);
    d.uncovered = img.at(58, 6);
  });
  return d;
}
}

TEST_CASE(
    "a PER_MIP iteration writes the depth output its level 0 matches",
    "[gfx][raster][depth][R31]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const auto single = renderDepth(api, "");
  if(single.skipped)
    SKIP("backend unavailable");
  INFO(single.err);
  REQUIRE(single.err.empty());
  const auto mip = renderDepth(
      api, R"( "EXECUTION_MODEL": { "TYPE": "PER_MIP", "TARGET": "target" },)");
  INFO(mip.err);
  REQUIRE(mip.err.empty());
  INFO(
      "single pass: covered " << int(single.covered[0]) << " uncovered "
                              << int(single.uncovered[0]) << "; PER_MIP: covered "
                              << int(mip.covered[0]) << " uncovered "
                              << int(mip.uncovered[0]));
  // The single pass writes the triangle's depth (z 0.5: 0.5 on Vulkan, 0.75
  // on GL) over the clear value.
  REQUIRE(single.covered[0] != single.uncovered[0]);
  CHECK(std::abs(int(mip.covered[0]) - int(single.covered[0])) <= 3);
  CHECK(std::abs(int(mip.uncovered[0]) - int(single.uncovered[0])) <= 3);
}

// A PER_LAYER colour target attaches layer i of a depth array of as many
// layers: the iterations render, depth-tested, without a render-pass mismatch.
TEST_CASE(
    "a PER_LAYER iteration attaches its layer of the depth output",
    "[gfx][raster][depth][R31]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("layer-depth.fs");
  {
    QFile f{fs};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "target", "TYPE": "color", "FORMAT": "rgba8", "WIDTH": 64, "HEIGHT": 64, "LAYERS": 2 },
    { "NAME": "depth", "TYPE": "depth", "FORMAT": "d32f", "WIDTH": 64, "HEIGHT": 64, "LAYERS": 2 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "target" },
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true, "DEPTH_WRITE": true,
    "CULL_MODE": "none", "TOPOLOGY": "triangles", "VERTEX_COUNT": 3
  }
}*/
void main() { isf_FragColor = vec4(1.0); }
)_");
  }
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api, {}, corpus("rr-msaa-edge.vs"), fs, {64, 64}, 3);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  REQUIRE(r.outputs[0].valid());
  // Layer 0 of the colour target has the triangle.
  CHECK(r.outputs[0].at(6, 32)[0] > 240);
  CHECK(r.outputs[0].at(58, 6)[0] < 16);
}
