// A raw raster's INPUTS storage_input wired through a Buffer edge binds the
// producer's buffer, and no adoption reference outlives the graph.
//
// INPUTS storage flows through m_storage and bindUpstreamBuffers, which finds
// the upstream by the entry's input port index. That index was computed from
// port 0, while a raw raster's port 0 is its geometry input, so every entry
// named the port before its own and a Buffer edge was never found.
// initState() also carried try_bind_from_input_port, and update() a matching
// loop over AuxiliarySSBO::input_port_index, which the raw raster never
// assigns; both were unreachable.
//
// Registration:
//   score_add_gfx_test(raw_raster_buffer_input GfxRawRasterBufferInput.cpp)
//
//   DISPLAY=:0 SCORE_TESTS_NO_XVFB=1 SCORE_TEST_API=vulkan \
//     ctest -R gfx_raw_raster_buffer_input
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE(
    "a raw-raster storage_input fed by a Buffer edge binds it and releases it",
    "[gfx][l3][binding][lifetime]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  bool built = false;
  bool skipped = false;
  std::string err;
  int before = -1, after = -1;
  std::array<uint8_t, 4> centre{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    before = score::gfx::RenderList::adoptedBufferCount();
    {
      GfxPipeline p;
      const int producer = p.addCsf(corpus("syn-storage-colour.cs"));
      const int consumer = p.addRaster(
          corpus("rr-storage-input.vs"), corpus("rr-storage-input.fs"));
      if(producer < 0 || consumer < 0)
      {
        err = "node build failed: " + p.error();
        return;
      }
      auto* out = p.bufferOut(producer, 0);
      auto* in = p.bufferIn(consumer, 0);
      if(!out || !in)
      {
        err = "expected a Buffer port on each side";
        return;
      }
      p.wire(out, in);
      const int sink = p.addSink({32, 32});
      p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));

      if(!p.create(be))
      {
        skipped = p.skipped();
        err = skipped ? std::string{} : p.error();
        return;
      }
      built = true;
      p.render(4);
      const auto img = p.readback(sink);
      if(!img.valid())
      {
        err = "readback failed";
        return;
      }
      centre = img.at(img.width / 2, img.height / 2);
    }
    after = score::gfx::RenderList::adoptedBufferCount();
  });

  if(skipped)
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(be) << " error=" << err);
  INFO(
      "centre=(" << int(centre[0]) << "," << int(centre[1]) << ","
                 << int(centre[2]) << ")");
  INFO("adopted before=" << before << " after=" << after);
  REQUIRE(err.empty());
  REQUIRE(built);

  CHECK(centre[0] < 40);
  CHECK(centre[1] > 215);
  CHECK(centre[2] < 40);
  CHECK(after == before);
}

// The same cable connected while the graph is already rendering. The raw
// raster's passes, and their SRBs, exist before the producer's buffer does,
// so the buffer only reaches the shader if the refresh in update() patches
// the SRBs it finds.
TEST_CASE(
    "a raw-raster storage_input wired to a Buffer edge after the first frames "
    "picks the buffer up",
    "[gfx][l3][binding][incremental]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  bool built = false;
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> unwired{}, wired{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int consumer = p.addRaster(
        corpus("rr-storage-input.vs"), corpus("rr-storage-input.fs"));
    const int producer = p.addCsf(corpus("syn-storage-colour.cs"));
    if(producer < 0 || consumer < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    built = true;
    p.render(3);
    const auto img1 = p.readback(sink);
    if(!img1.valid())
    {
      err = "readback failed before wiring";
      return;
    }
    unwired = img1.at(img1.width / 2, img1.height / 2);

    p.addEdgeIncremental(p.bufferOut(producer, 0), p.bufferIn(consumer, 0));
    p.render(4);
    const auto img2 = p.readback(sink);
    if(!img2.valid())
    {
      err = "readback failed after wiring";
      return;
    }
    wired = img2.at(img2.width / 2, img2.height / 2);
  });

  if(skipped)
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(be) << " error=" << err);
  INFO(
      "unwired=(" << int(unwired[0]) << "," << int(unwired[1]) << ","
                  << int(unwired[2]) << ") wired=(" << int(wired[0]) << ","
                  << int(wired[1]) << "," << int(wired[2]) << ")");
  REQUIRE(err.empty());
  REQUIRE(built);

  CHECK(unwired[1] < 40);
  CHECK(wired[0] < 40);
  CHECK(wired[1] > 215);
  CHECK(wired[2] < 40);
}
