// A PERSISTENT storage image reads last frame's contents as <name>_prev while
// the passes write <name>. On the first frame there is no last frame: _prev
// was bound to the very image being written (shader board C21), so a shader
// reading _prev at a texel it already wrote this frame saw its own output.
// The history of the first frame must be a separate image, zero-filled.
//
// csf-persistent-image-shift.cs shifts an 8x1 row right by one texel per frame
// from one invocation walking left to right: frame k must light exactly
// texels 0 .. k-1.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <vector>

using namespace score::test::gfx;

namespace
{
int lit_prefix(const ReadbackImage& img)
{
  int n = 0;
  for(int x = 0; x < img.width; x++)
  {
    // The 8x1 image is stretched over the sink: sample the middle row.
    const auto p = img.at(x * img.width / 8 + img.width / 16, img.height / 2);
    if(p[0] < 128)
      break;
    n++;
  }
  return n;
}
}

TEST_CASE(
    "a PERSISTENT image's first-frame history is not the image being written",
    "[gfx][csf][persistent][C21]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  struct
  {
    bool skipped{};
    std::string error;
    std::vector<ReadbackImage> shots;
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int cs = p.addCsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-persistent-image-shift.cs"));
    if(cs < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(cs, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(1);
    r.shots.push_back(p.readback(sink));
    p.render(2);
    r.shots.push_back(p.readback(sink));
    if(r.error.empty())
      r.error = p.error();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.shots.size() == 2);
  REQUIRE(r.shots[0].valid());
  REQUIRE(r.shots[1].valid());
  CHECK(lit_prefix(r.shots[0]) == 1);
  CHECK(lit_prefix(r.shots[1]) == 3);
}
