// A scene mesh whose index buffer is an upstream GPU buffer draws the
// triangles its indices name. This is what PBR Mesh publishes for any indexed
// geometry that reached it on the GPU (Avendish upload of a CPU index, a CSF
// forwarding its upstream index, or a Scene Preprocessor's own output).
//
// The scene has two meshes so the GPU-indexed one lands at a non-zero base
// vertex and first index in the arena:
//  - A: CPU positions, not indexed, a quad over the right half.
//  - B: positions and indices in one GPU buffer, the indices after the
//    positions (so the index buffer carries a byte offset, as PBR Mesh sets
//    it). Five vertices; the three indices {0, 4, 3} name one triangle with
//    its left edge on the quad's left side and its apex at the middle of the
//    right side. Drawing the vertices in order instead would cover the
//    bottom-right corner, and a wrong base vertex would read A's vertices.
#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstring>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;
constexpr int kIndexOffset = 5 * 16;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

ossia::vertex_attribute positionAttr()
{
  ossia::vertex_attribute a;
  a.semantic = ossia::attribute_semantic::position;
  a.format = ossia::vertex_format::float4;
  a.buffer_index = 0;
  a.byte_offset = 0;
  a.byte_stride = 16;
  a.rate = ossia::vertex_attribute::input_rate::per_vertex;
  return a;
}

ossia::scene_payload meshPayload(ossia::mesh_primitive prim)
{
  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->bounds = prim.bounds;
  mesh->primitives.push_back(std::move(prim));
  mesh->dirty_index = 1;
  return ossia::mesh_component_ptr(std::move(mesh));
}

std::shared_ptr<ossia::scene_state>
makeState(QRhiBuffer* gpu, int64_t gpuBytes, ossia::index_format fmt)
{
  // A: CPU, not indexed.
  ossia::mesh_primitive a;
  {
    const float x0 = 0.1f, x1 = 0.9f, y0 = -0.8f, y1 = 0.8f;
    a.vertex_buffers.push_back(scene::cpu_buffer(
        {x0, y0, 0, 1, x1, y0, 0, 1, x1, y1, 0, 1,
         x0, y0, 0, 1, x1, y1, 0, 1, x0, y1, 0, 1},
        ossia::buffer_data::usage::vertex_buffer));
    a.attributes.push_back(positionAttr());
    a.topology = ossia::primitive_topology::triangles;
    a.vertex_count = 6;
    a.stable_id = 0x1D0C0001u;
    a.bounds = {{x0, y0, 0.f}, {x1, y1, 0.f}};
  }

  // B: GPU positions and GPU indices in the same buffer.
  ossia::mesh_primitive b;
  {
    auto vbo = std::make_shared<ossia::buffer_resource>();
    ossia::gpu_buffer_handle vh;
    vh.native_handle = gpu;
    vh.byte_size = gpuBytes;
    vbo->resource = vh;
    vbo->dirty_index = 1;
    b.vertex_buffers.push_back(vbo);
    b.attributes.push_back(positionAttr());

    auto ibo = std::make_shared<ossia::buffer_resource>();
    ossia::gpu_buffer_handle ih = vh;
    ih.byte_offset = kIndexOffset;
    ibo->resource = ih;
    ibo->dirty_index = 1;
    b.index_buffer = ibo;
    b.index_type = fmt;
    b.index_count = 3;

    b.topology = ossia::primitive_topology::triangles;
    b.vertex_count = 5;
    b.stable_id = 0x1D0C0002u;
    b.bounds = {{-0.9f, -0.8f, 0.f}, {-0.1f, 0.8f, 0.f}};
  }

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(meshPayload(std::move(a)));
  children->push_back(meshPayload(std::move(b)));
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::move(children);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(root));

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

struct GpuIndexNode final : score::gfx::ProcessNode
{
  ossia::index_format format{};

  explicit GpuIndexNode(ossia::index_format f)
      : format{f}
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct GpuIndexRenderer final : score::test::gfx::scene::SourceRenderer
{
  const GpuIndexNode& self;
  QRhiBuffer* m_buf{};

  explicit GpuIndexRenderer(const GpuIndexNode& n)
      : SourceRenderer{n}
      , self{n}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    const float x0 = -0.9f, x1 = -0.1f, y0 = -0.8f, y1 = 0.8f;
    const float pos[20] = {x0, y0, 0, 1, x1, y0, 0, 1, x1, y1, 0, 1,
                           x0, y1, 0, 1, x1, 0,  0, 1};
    std::vector<std::byte> data(kIndexOffset + 16);
    std::memcpy(data.data(), pos, sizeof(pos));
    if(self.format == ossia::index_format::uint16)
    {
      const uint16_t idx[3] = {0, 4, 3};
      std::memcpy(data.data() + kIndexOffset, idx, sizeof(idx));
    }
    else
    {
      const uint32_t idx[3] = {0, 4, 3};
      std::memcpy(data.data() + kIndexOffset, idx, sizeof(idx));
    }
    m_buf = r.state.rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::VertexBuffer | QRhiBuffer::IndexBuffer
            | QRhiBuffer::StorageBuffer,
        (quint32)data.size());
    m_buf->setName("gpu_pos_and_indices");
    m_buf->create();
    res.uploadStaticBuffer(m_buf, data.data());
    m_scene.state = makeState(m_buf, (int64_t)data.size(), self.format);
    m_initialized = true;
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }

  void release(score::gfx::RenderList& r) override
  {
    SourceRenderer::release(r);
    delete m_buf;
    m_buf = nullptr;
  }
};

score::gfx::NodeRenderer*
GpuIndexNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new GpuIndexRenderer{*this};
}
}

TEST_CASE(
    "a scene mesh with a GPU index buffer draws the triangles its indices name",
    "[gfx][scene][gpu-indices]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto fmt = GENERATE(ossia::index_format::uint16, ossia::index_format::uint32);
  CAPTURE(backend_name(api));
  CAPTURE(fmt == ossia::index_format::uint16 ? "uint16" : "uint32");

  bool built = false;
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> inside{}, topRight{}, bottomRight{}, other{};
  std::array<uint8_t, 4> insideLater{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addNode(std::make_unique<GpuIndexNode>(fmt));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus("syn-scene-gpu-pos.vs"), corpus("syn-scene-gpu-pos.fs"));
    if(src < 0 || flat < 0 || raster < 0)
    {
      err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(src, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
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
    inside = img.at(8, kSize / 2);
    topRight = img.at(26, 8);
    bottomRight = img.at(26, kSize - 8);
    other = img.at(3 * kSize / 4, kSize / 2);

    // The copy is re-issued on the frames that reuse the arena slab.
    p.render(4);
    const auto img2 = p.readback(sink);
    if(!img2.valid())
    {
      err = "second readback failed";
      return;
    }
    insideLater = img2.at(8, kSize / 2);
  });

  if(skipped)
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(api) << " error=" << err);
  INFO(
      "B inside g=" << int(inside[1]) << " B top-right g=" << int(topRight[1])
                    << " B bottom-right g=" << int(bottomRight[1])
                    << " A g=" << int(other[1])
                    << " B inside later g=" << int(insideLater[1]));
  REQUIRE(err.empty());
  REQUIRE(built);

  CHECK(other[1] > 215);
  CHECK(inside[1] > 215);
  CHECK(topRight[1] < 40);
  CHECK(bottomRight[1] < 40);
  CHECK(insideLater[1] > 215);
}

TEST_CASE(
    "the Scene Preprocessor copies 16-bit GPU indices everywhere but on Metal",
    "[gfx][scene][index]")
{
  using score::gfx::copiesGpuIndices;
  std::vector<QRhi::Implementation> apis{
      QRhi::Vulkan, QRhi::OpenGLES2, QRhi::D3D11, QRhi::Null};
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  apis.push_back(QRhi::D3D12);
#endif
  for(auto api : apis)
  {
    CAPTURE(int(api));
    CHECK(copiesGpuIndices(api, ossia::index_format::uint16));
    CHECK(copiesGpuIndices(api, ossia::index_format::uint32));
  }
  // Metal on macOS only blits 4-byte-aligned ranges, and a uint16 index is
  // copied into each uint32 slot two bytes at a time.
  CHECK(!copiesGpuIndices(QRhi::Metal, ossia::index_format::uint16));
  CHECK(copiesGpuIndices(QRhi::Metal, ossia::index_format::uint32));
}
