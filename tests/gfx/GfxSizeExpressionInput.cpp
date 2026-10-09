// An ISF pass and a CSF image sized by an expression that reads an input
// follow that input when it changes while running.
//
// Each shader paints the width of what it sized, divided by 255, in red: 16
// at the default, then 48 once the `size` input is set to 48.
#include "IsfTestCommon.hpp"

#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shots
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  std::vector<ReadbackImage> images;
};

Shots run(score::gfx::GraphicsApi backend, bool compute)
{
  Shots s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = compute ? p.addCsf(corpus("csf-image-size-from-input.cs"))
                             : p.addIsf(corpus("isf-pass-size-from-input.fs"));
    const int sink = p.addSink({64, 64});
    if(node < 0)
    {
      s.error = "build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      s.skipped = p.skipped();
      s.skip_reason = p.skipReason();
      s.error = p.error();
      return;
    }
    s.backend = p.backend();
    p.render(3);
    s.images.push_back(p.readback(sink));
    const int sizePort = score::test::gfx::nth_control_input(*p.isf(node), 0);
    if(sizePort < 0)
    {
      s.error = "no control inlet for `size`";
      return;
    }
    score::test::gfx::setControl(*p.isf(node), sizePort, ossia::value{48});
    p.render(3);
    s.images.push_back(p.readback(sink));
    if(s.error.empty())
      s.error = p.error();
  });
  return s;
}
}

TEST_CASE(
    "an ISF pass and a CSF image sized from an input follow the input",
    "[gfx][isf][csf][size-expression]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const bool compute = GENERATE(false, true);
  CAPTURE(backend_name(backend), compute);
  const Shots s = run(backend, compute);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  if(compute)
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());
  REQUIRE(s.images.size() == 2);
  REQUIRE(s.images[0].valid());
  REQUIRE(s.images[1].valid());

  // The pixel at (4, 4) lies inside the image at both sizes.
  const int before = s.images[0].at(4, 4)[0];
  const int after = s.images[1].at(4, 4)[0];
  INFO("width read before " << before << " after " << after);
  CHECK(before == 16);
  CHECK(after == 48);
}
