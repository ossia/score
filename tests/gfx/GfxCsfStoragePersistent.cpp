// A PERSISTENT storage buffer in a CSF reads, as `<name>_prev`, what the
// buffer held when the previous frame's passes finished.
//
// csf-storage-persistent-counter.cs stores counter_prev + 1 every frame and
// paints counter_prev in red: across N frames the red channel grows by N.
// Without the `_prev` binding the shader does not compile.
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
}

TEST_CASE(
    "a CSF PERSISTENT storage buffer reads the previous frame as _prev",
    "[gfx][csf][storage][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shots s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int cs = p.addCsf(corpus("csf-storage-persistent-counter.cs"));
    const int sink = p.addSink({16, 16});
    if(cs < 0)
    {
      s.error = "csf build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(cs, 0), p.sinkInput(sink));
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
    p.render(5);
    s.images.push_back(p.readback(sink));
    if(s.error.empty())
      s.error = p.error();
  });
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  // Asked after the run: it opens a GL surface, which needs the application.
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());
  REQUIRE(s.images.size() == 2);
  REQUIRE(s.images[0].valid());
  REQUIRE(s.images[1].valid());

  const int before = s.images[0].center()[0];
  const int after = s.images[1].center()[0];
  INFO("red before " << before << " after " << after);
  CHECK(before >= 1);
  CHECK(after - before == 5);
}
