// A CSF that modifies its upstream's geometry works on a copy unless it asks
// otherwise (agent A5, N59 remainder).
//
// csf-a5-initonce-producer.cs writes its geometry on frame 0 only. Before, a
// read_write consumer worked in place on any buffer a CSF dispatch had written
// this frame or the last, and every dispatch counted as a write, so the
// consumer's +0.1 accumulated in the producer's buffer frame after frame.
// Pinned here, each rendered for 3 and then 12 frames:
// - a read_write attribute works on a copy refreshed every frame: 0.3 both times;
// - with "PERSISTENT": true on the geometry resource it works in place and
//   accumulates;
// - a read_write auxiliary works on a copy too: 0.3 both times;
// Feedback loops keep working in place (GfxCsfFeedbackLoop).
//
// Registration:
//   score_add_gfx_test(csf_read_write_copy_a5 GfxCsfReadWriteCopyA5.cpp)
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct Shot
{
  bool skipped = false;
  std::string error;
  std::array<uint8_t, 4> early{}, late{};
};

Shot renderChain(score::gfx::GraphicsApi api, const char* consumer)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addCsf(corpus("csf-a5-initonce-producer.cs"));
    const int cons = p.addCsf(corpus(consumer));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    if(producer < 0 || cons < 0 || raster < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.geometryOut(producer, 0), p.geometryIn(cons, 0));
    p.wire(p.geometryOut(cons, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    auto img = p.readback(sink);
    if(!img.valid())
    {
      r.error = "readback failed";
      return;
    }
    r.early = img.center();
    p.render(9);
    img = p.readback(sink);
    if(!img.valid())
    {
      r.error = "readback failed";
      return;
    }
    r.late = img.center();
  });
  return r;
}
}

TEST_CASE("a read_write CSF on an init-once upstream works on a copy", "[gfx][csf][n59]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string consumer = GENERATE(
      "csf-a5-rw-consumer.cs", "csf-a5-rw-consumer-persistent.cs",
      "csf-a5-aux-rw-consumer.cs");
  CAPTURE(backend_name(api), consumer);

  const Shot s = renderChain(api, consumer.c_str());
  if(s.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  INFO("early " << int(s.early[0]) << " late " << int(s.late[0]));
  const int once = int(0.3 * 255 + 0.5);
  if(consumer == "csf-a5-rw-consumer-persistent.cs")
  {
    CHECK(int(s.early[0]) > once + 20);
    CHECK(int(s.late[0]) > int(s.early[0]) + 100);
  }
  else
  {
    CHECK(std::abs(int(s.early[0]) - once) <= 2);
    CHECK(std::abs(int(s.late[0]) - once) <= 2);
  }
}
