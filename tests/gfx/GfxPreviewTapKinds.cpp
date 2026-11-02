// Previews of textures that hold several images: a tap on a 3D texture, an
// array or a cube map shows all of them, tiled as the Texture view's Overview
// lays them out (score::gfx::ShownTexture), and samples each with a sampler
// of its kind.
//
//     ctest -R gfx_preview_tap_kinds --output-on-failure

#include <Gfx/Graph/PreviewTap.hpp>
#include <Gfx/Graph/TextureKind.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <score_test/Gfx.hpp>

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

using score::test::gfx::near;
using score::test::gfx::ReadbackImage;

struct TapRun
{
  bool skipped{};
  std::string skip_reason;
  std::string error;
  ReadbackImage tap;
  score::gfx::TextureShape shape;
};

// The compute shader `shader` writes its image, which a tap on that output
// reads back, while an offscreen output of the patch renders it.
TapRun run(score::gfx::GraphicsApi api, const char* shader, QSize tapSize)
{
  TapRun r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(api == score::gfx::Null)
    {
      r.skipped = true;
      r.skip_reason = score::test::gfx::null_backend_skip_reason();
      return;
    }
    if(const char* why = score::test::gfx::compute_shader_skip_reason(api))
    {
      r.skipped = true;
      r.skip_reason = why;
      return;
    }
    std::string probed;
    if(!score::test::gfx::probe_api(api, probed))
    {
      r.skipped = true;
      r.skip_reason = std::string(score::test::gfx::backend_name(api)) + " unavailable";
      return;
    }

    auto built = score::test::gfx::make_isf_node(corpus(shader));
    if(!built.node)
    {
      r.error = built.error;
      return;
    }
    auto& node = *built.node;
    auto* image = score::test::gfx::nth_image_output(node, 0);
    if(!image)
    {
      r.error = "no image output";
      return;
    }
    const int port = int(
        std::find(node.output.begin(), node.output.end(), image) - node.output.begin());

    auto out = std::make_unique<score::gfx::BackgroundNode>();
    out->shared_readback = std::make_shared<QRhiReadbackResult>();
    out->setSize({64, 64});

    auto tap = std::make_shared<score::gfx::PreviewTap>();
    tap->port = port;
    tap->size = tapSize;
    tap->active = true;
    tap->readback = std::make_shared<QRhiReadbackResult>();
    node.previewTaps.push_back(tap);

    {
      score::gfx::Graph graph;
      node.nodeId = 1;
      out->nodeId = 2;
      graph.addNode(&node);
      graph.addNode(out.get());
      graph.addEdge(image, out->input[0], Process::CableType::ImmediateGlutton);
      graph.createAllRenderLists(api);
      if(!out->renderState())
      {
        r.skipped = true;
        r.skip_reason = "no offscreen render state";
        return;
      }
      for(int f = 0; f < 4; f++)
        score::test::gfx::pump_frame({&node}, {out.get()}, f, 4);
      out->renderState()->rhi->finish();

      for(auto& [rl, rn] : node.renderedNodes)
        if(rl && rn)
          if(auto* tex = rn->textureForOutput(*image))
            r.shape = score::gfx::TextureShape::of(*tex);

      const auto& rb = *tap->readback;
      r.tap.width = rb.pixelSize.width();
      r.tap.height = rb.pixelSize.height();
      r.tap.bytes = rb.data;
    }
    node.previewTaps.clear();
  });
  return r;
}

//! The pixel at the centre of the tile at `col`, `row` of a grid of `cols`
//! by `rows`.
std::array<uint8_t, 4>
cell(const ReadbackImage& img, int col, int row, int cols, int rows)
{
  return img.at(
      std::clamp(int((col + 0.5) * img.width / cols), 0, img.width - 1),
      std::clamp(int((row + 0.5) * img.height / rows), 0, img.height - 1));
}

std::array<uint8_t, 4> rgb(double r, double g, double b)
{
  return {
      uint8_t(std::lround(r * 255)), uint8_t(std::lround(g * 255)),
      uint8_t(std::lround(b * 255)), 255};
}

#define SKIP_OR_REQUIRE(r)    \
  if(r.skipped)               \
    SKIP(r.skip_reason);      \
  INFO("error: " << r.error); \
  REQUIRE(r.error.empty());   \
  REQUIRE(r.tap.valid())
}

TEST_CASE("a tap on a texture array tiles its layers", "[gfx][preview_tap][kinds]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "csf-array-image-write.cs", {256, 256});
  SKIP_OR_REQUIRE(r);
  CHECK(r.shape.kind == score::gfx::TextureKind::Array);
  CHECK(r.shape.layers == 4);
  // Four 32x32 layers in two columns and two rows
  CHECK(r.tap.width == 64);
  CHECK(r.tap.height == 64);
  // From the top left, row by row: each layer's own tint
  CHECK(near(cell(r.tap, 0, 0, 2, 2), rgb(0.25, 0.25, 0.85), 3));
  CHECK(near(cell(r.tap, 1, 0, 2, 2), rgb(0.85, 0.25, 0.25), 3));
  CHECK(near(cell(r.tap, 0, 1, 2, 2), rgb(0.25, 0.85, 0.25), 3));
  CHECK(near(cell(r.tap, 1, 1, 2, 2), rgb(0.85, 0.85, 0.25), 3));
}

TEST_CASE(
    "a tap on a 3D texture tiles slices across its depth", "[gfx][preview_tap][kinds]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "csf-3d-image-write.cs", {256, 256});
  SKIP_OR_REQUIRE(r);
  CHECK(r.shape.kind == score::gfx::TextureKind::Texture3D);
  CHECK(r.shape.layers == 64);
  // Sixteen 64x64 slices, four by four
  CHECK(r.tap.width == 256);
  CHECK(r.tap.height == 256);
  // Blue is the depth of the slice: the first, then evenly to the last
  const auto shown = score::gfx::ShownTexture::of(
      r.shape, score::gfx::TextureLayout::Overview, 0., 0);
  REQUIRE(shown.tiles() == 16);
  for(int i = 0; i < 16; i++)
  {
    CAPTURE(i);
    const int z = shown.tileLayer(i);
    const auto px = cell(r.tap, i % 4, i / 4, 4, 4);
    CHECK(std::abs(int(px[2]) - int(std::lround((z + 0.5) / 64. * 255.))) <= 3);
  }
  CHECK(shown.tileLayer(0) == 0);
  CHECK(shown.tileLayer(15) == 63);
}

TEST_CASE("a tap on a cube map unfolds it as a cross", "[gfx][preview_tap][kinds]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));
  const auto r = run(api, "csf-cube-image-write.cs", {256, 256});
  SKIP_OR_REQUIRE(r);
  CHECK(r.shape.kind == score::gfx::TextureKind::Cube);
  // Six 32x32 faces in a cross four faces wide and three high
  CHECK(r.tap.width == 128);
  CHECK(r.tap.height == 96);
  // The tint of face f: 0.25 + 0.6 for each of the bits of f
  auto face = [](int f) {
    return rgb(
        0.25 + 0.6 * (f & 1), 0.25 + 0.6 * ((f >> 1) & 1), 0.25 + 0.6 * ((f >> 2) & 1));
  };
  CHECK(near(cell(r.tap, 1, 0, 4, 3), face(2), 3)); // +Y
  CHECK(near(cell(r.tap, 0, 1, 4, 3), face(1), 3)); // -X
  CHECK(near(cell(r.tap, 1, 1, 4, 3), face(4), 3)); // +Z
  CHECK(near(cell(r.tap, 2, 1, 4, 3), face(0), 3)); // +X
  CHECK(near(cell(r.tap, 3, 1, 4, 3), face(5), 3)); // -Z
  CHECK(near(cell(r.tap, 1, 2, 4, 3), face(3), 3)); // -Y
  // Around the cross
  const auto gap = rgb(0.12, 0.12, 0.12);
  CHECK(near(cell(r.tap, 0, 0, 4, 3), gap, 3));
  CHECK(near(cell(r.tap, 3, 2, 4, 3), gap, 3));
}

TEST_CASE("the layout of the views", "[gfx][kinds]")
{
  using namespace score::gfx;
  const TextureShape cube{TextureKind::Cube, 512, 512, 6};
  const auto cross = ShownTexture::of(cube, TextureLayout::Overview, 0., 0);
  CHECK(cross.size(8192) == QSize(2048, 1536));
  CHECK(cross.locate(0.375, 0.5).layer == 4);
  CHECK(cross.locate(0.625, 0.5).layer == 0);
  CHECK(cross.locate(0.125, 0.1).layer == -1);
  CHECK(cube.description() == QStringLiteral("cube 512"));
  CHECK(cube.layerName(1) == QStringLiteral("face -X"));

  const TextureShape volume{TextureKind::Texture3D, 64, 64, 32};
  CHECK(volume.description() == QStringLiteral("3D 64x64x32"));
  CHECK(ShownTexture::of(volume, TextureLayout::Single, 1., 0).index == 31);
  CHECK(ShownTexture::of(volume, TextureLayout::Single, 0.5, 0).index == 16);
  const auto tiles = ShownTexture::of(volume, TextureLayout::Overview, 0., 0);
  CHECK(tiles.columns() == 4);
  CHECK(tiles.rows() == 4);
  CHECK(tiles.size(128) == QSize(128, 128));

  const TextureShape array{TextureKind::Array, 256, 256, 8};
  CHECK(array.description() == QStringLiteral("array 8 layers 256x256"));
  CHECK(ShownTexture::of(array, TextureLayout::Single, 0., 12).index == 7);
  const auto layers = ShownTexture::of(array, TextureLayout::Overview, 0., 0);
  CHECK(layers.columns() == 3);
  CHECK(layers.rows() == 3);
  CHECK(layers.locate(0.9, 0.9).layer == -1);
  CHECK(layers.locate(0.5, 0.5).layer == 4);

  CHECK(TextureShape::unpack(array.pack()) == array);
  CHECK(TextureShape::unpack(cube.pack()) == cube);
}
