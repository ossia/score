// Pins a raw raster shaped like volumetric_composite: a geometry INPUT with no
// attributes next to a DEPTH:true image input, a uniform block, material
// controls and a top-level AUXILIARY buffer. A raw raster's geometry comes in
// through its one implicit Geometry port (the score model has no other), so the
// declared geometry input must not add node ports: an extra Geometry inlet
// shifted every later port against the model's, and the extra Geometry outlet
// took output 0, where the image output belongs. The draw then shows the
// controls and the cabled colour.
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  IsfResult result;
  std::array<uint8_t, 4> px{};
  int geometryInlets{-1};
  std::vector<score::gfx::Types> outputs;
};

Shot renderDepthInput(score::gfx::GraphicsApi backend, bool cabled, float level, int mode)
{
  Shot shot;
  auto& r = shot.result;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r.backend = backend_name(backend);
    GfxPipeline p;
    const int rr
        = p.addRaster(corpus("e2-rr-depth-input.vs"), corpus("e2-rr-depth-input.fs"));
    const int src = cabled ? p.addIsf(corpus("a5-solid-green.fs")) : -1;
    if(rr < 0 || (cabled && src < 0))
    {
      r.error = p.error();
      return;
    }
    auto& node = *p.isf(rr);
    shot.geometryInlets = 0;
    for(auto* in : node.input)
      if(in->type == score::gfx::Types::Geometry)
        shot.geometryInlets++;
    for(auto* out : node.output)
      shot.outputs.push_back(out->type);

    const int s = p.addSink({32, 32});
    if(cabled)
      p.wire(p.imageOut(src, 0), p.imageIn(rr, 0));
    p.wire(p.imageOut(rr, 0), p.sinkInput(s));
    if(!p.create(backend))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    std::vector<int> controls;
    for(int k = 0, port = 0; (port = nth_control_input(node, k)) >= 0; ++k)
      controls.push_back(port);
    if(controls.size() < 2)
    {
      r.error = "missing controls";
      return;
    }
    setControl(node, controls[controls.size() - 2], level);
    setControl(node, controls.back(), mode);
    p.render(3);
    ReadbackImage img = p.readback(s);
    if(!img.valid())
    {
      r.error = "readback empty";
      return;
    }
    shot.px = img.at(16, 16);
    r.outputs.push_back(std::move(img));
  });
  return shot;
}
}

TEST_CASE(
    "Raw raster with a geometry input and a DEPTH:true image input keeps its "
    "port layout, controls and colour",
    "[gfx][raster][depth][material]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const bool cabled = GENERATE(false, true);
  const int mode = GENERATE(0, 1);
  CAPTURE(backend_name(backend), cabled, mode);

  const Shot s = renderDepthInput(backend, cabled, 0.75f, mode);
  CHECK(s.geometryInlets == 1);
  CHECK(s.outputs == std::vector<score::gfx::Types>{score::gfx::Types::Image});
  if(s.result.skipped)
    SKIP(s.result.backend + ": " + s.result.skip_reason);
  REQUIRE(s.result.error.empty());
  CAPTURE(int(s.px[0]), int(s.px[1]), int(s.px[2]), int(s.px[3]));
  CHECK(std::abs(int(s.px[0]) - 191) <= 2);
  CHECK(int(s.px[1]) == (mode == 1 ? 255 : 0));
  CHECK(int(s.px[2]) == (cabled ? 255 : 0));
  CHECK(int(s.px[3]) == 255);
}
