// A raster whose vertex shader reads no attribute (it works from
// gl_VertexIndex / gl_InstanceIndex and storage buffers) still receives the
// upstream geometry, e.g. a Scene Preprocessor output carrying the camera and
// lights. Its pipeline must not keep the geometry's vertex bindings: the draw
// binds one buffer per binding in the layout, and on Metal those buffers start
// past the shader's own buffers in a 31-slot table. A vertex shader that calls
// .length() on a storage buffer gets SPIRV-Cross's buffer-size buffer at slot
// 25, so eight mesh streams would end at slot 33 and the pipeline is refused
// (particle_sprites over a Scene Preprocessor drew nothing on Metal).
//
// Eight streams, a shader with no inputs: the resolver leaves no binding in
// the layout and hands the draw an empty, compacted plan, on every backend.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
constexpr int kStreamCount = 8;

ossia::geometry makeEightStreamGeometry()
{
  ossia::geometry geom;
  for(int i = 0; i < kStreamCount; ++i)
  {
    geom.bindings.push_back({16, ossia::geometry::binding::per_vertex, 0});
    ossia::geometry::attribute a;
    a.binding = i;
    a.location = i;
    a.format = ossia::geometry::attribute::float4;
    a.byte_offset = 0;
    a.semantic = i == 0 ? ossia::attribute_semantic::position
                        : ossia::attribute_semantic::color0;
    geom.attributes.push_back(a);
  }
  return geom;
}

int bindingCount(const QRhiVertexInputLayout& l)
{
  return int(std::distance(l.cbeginBindings(), l.cendBindings()));
}
}

TEST_CASE(
    "a vertex shader with no inputs keeps none of the geometry's vertex bindings",
    "[gfx][gpu][vertex-input]")
{
  using namespace score::gfx;
  using namespace score::test::gfx;

  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool ran = false;
  bool remapped = false;
  bool created = false;
  int nb = -1;
  FallbackBindingPlan plan;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(16, 16), nullptr);
    if(!state || !state->rhi)
    {
      if(state)
        state->destroy();
      return;
    }
    auto& rhi = *state->rhi;
    {
      const auto shaders = makeShaders(*state, QStringLiteral(R"_(#version 450
layout(std430, binding = 0) readonly buffer pool_t { vec4 data[]; } pool;
layout(location=0) out vec4 v_col;
void main()
{
  uint n = uint(pool.data.length());
  uint i = uint(gl_InstanceIndex);
  v_col = i < n ? pool.data[i] : vec4(0.0);
  gl_Position = vec4(float(gl_VertexIndex % 3) - 1.0, 0.0, 0.0, 1.0);
}
)_"),
          QStringLiteral(R"_(#version 450
layout(location=0) in vec4 v_col;
layout(location=0) out vec4 frag;
void main() { frag = v_col; }
)_"));

      std::unique_ptr<QRhiBuffer> pool(rhi.newBuffer(
          QRhiBuffer::Immutable, QRhiBuffer::StorageBuffer, 16));
      pool->create();
      std::unique_ptr<QRhiTexture> tex(
          rhi.newTexture(QRhiTexture::RGBA8, QSize(16, 16), 1, QRhiTexture::RenderTarget));
      tex->create();
      std::unique_ptr<QRhiTextureRenderTarget> rt(rhi.newTextureRenderTarget({tex.get()}));
      std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
      rt->setRenderPassDescriptor(rp.get());
      rt->create();
      std::unique_ptr<QRhiShaderResourceBindings> srb(rhi.newShaderResourceBindings());
      srb->setBindings({QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::VertexStage, pool.get())});
      srb->create();

      std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi.newGraphicsPipeline());
      pipeline->setShaderStages(
          {{QRhiShaderStage::Vertex, shaders.first},
           {QRhiShaderStage::Fragment, shaders.second}});
      pipeline->setShaderResourceBindings(srb.get());
      pipeline->setRenderPassDescriptor(rp.get());

      // The geometry's full binding set, as Mesh::preparePipeline seeds it.
      const auto geom = makeEightStreamGeometry();
      {
        QRhiVertexInputLayout seed;
        QList<QRhiVertexInputBinding> bindings;
        for(const auto& b : geom.bindings)
          bindings.append(QRhiVertexInputBinding{quint32(b.byte_stride)});
        seed.setBindings(bindings.cbegin(), bindings.cend());
        pipeline->setVertexInputLayout(seed);
      }

      remapped = remapPipelineVertexInputs(*pipeline, shaders.first, geom, &plan, &rhi);
      nb = bindingCount(pipeline->vertexInputLayout());
      created = pipeline->create();
      ran = true;
    }
    state->destroy();
  });

  if(!ran)
    SKIP(std::string{backend_name(api)} + " unavailable");
  CHECK(remapped);
  // No stream is read, so none is bound.
  CHECK(nb == 0);
  // The draw binds by the plan: compacted to nothing, not "every input".
  CHECK(plan.compacted);
  CHECK(plan.mesh_bindings.empty());
  CHECK(plan.slots.empty());
  CHECK(created);
}
