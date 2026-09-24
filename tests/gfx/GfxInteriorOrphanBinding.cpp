// A vertex binding no attribute reads is not merely wasteful: Metal refuses
// the descriptor outright --
//
//   remapPipelineVertexInputs(keep-bindings): vertex binding 1 (stride 16) has
//     no attribute reading it; Metal rejects such a layout
//   -[MTLVertexDescriptorInternal newSerializedDescriptor]:743:
//     failed assertion `Serialized Descriptor Creation'
//
// -- so the pipeline is never created and the node draws nothing. Dropping
// only the *trailing* orphans, which is what shipped, cannot reach a binding
// that sits between two the shader does read.
//
// This test builds exactly that shape: a geometry publishing three streams
// where the shader reads the first and the third. Binding 1 is the interior
// orphan. It asserts the compaction, the renumbering, the plan the draw needs
// to bind against, and -- the part that actually reproduced the crash --
// that the pipeline creates.
//
// Registration:
//   score_add_gfx_test(interior_orphan_binding GfxInteriorOrphanBinding.cpp)
//
//   DISPLAY=:0 SCORE_TESTS_NO_XVFB=1 SCORE_TEST_API=vulkan \
//     ctest -R gfx_interior_orphan_binding
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// position at binding 0, colour at binding 2. Binding 1 is published by the
// geometry and read by nothing.
ossia::geometry makeGeometryWithInteriorOrphan()
{
  ossia::geometry geom;
  geom.bindings.push_back({12, ossia::geometry::binding::per_vertex, 0});
  geom.bindings.push_back({16, ossia::geometry::binding::per_vertex, 0});
  geom.bindings.push_back({16, ossia::geometry::binding::per_vertex, 0});

  ossia::geometry::attribute pos;
  pos.binding = 0;
  pos.location = 0;
  pos.format = ossia::geometry::attribute::float3;
  pos.byte_offset = 0;
  pos.semantic = ossia::attribute_semantic::position;
  geom.attributes.push_back(pos);

  ossia::geometry::attribute col;
  col.binding = 2;
  col.location = 1;
  col.format = ossia::geometry::attribute::float4;
  col.byte_offset = 0;
  col.semantic = ossia::attribute_semantic::color0;
  geom.attributes.push_back(col);

  return geom;
}

int bindingCount(const QRhiVertexInputLayout& l)
{
  return int(std::distance(l.cbeginBindings(), l.cendBindings()));
}
}

TEST_CASE("interior orphan vertex bindings are compacted", "[gfx][gpu]")
{
  using namespace score::gfx;
  using namespace score::test::gfx;

  bool ran = false;
  std::string error;
  const auto api = platform_backends().front();
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(16, 16), nullptr);
    if(!state || !state->rhi)
    {
      error = "RHI unavailable";
      if(state)
        state->destroy();
      return;
    }
    auto& rhi = *state->rhi;
    {
      const auto shaders = makeShaders(*state, QStringLiteral(R"_(#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec4 color0;
layout(location=0) out vec4 v_col;
void main() { gl_Position = vec4(position, 1.0); v_col = color0; }
)_"),
          QStringLiteral(R"_(#version 450
layout(location=0) in vec4 v_col;
layout(location=0) out vec4 frag;
void main() { frag = v_col; }
)_"));

      std::unique_ptr<QRhiTexture> tex(
          rhi.newTexture(QRhiTexture::RGBA8, QSize(16, 16), 1, QRhiTexture::RenderTarget));
      tex->create();
      std::unique_ptr<QRhiTextureRenderTarget> rt(rhi.newTextureRenderTarget({tex.get()}));
      std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
      rt->setRenderPassDescriptor(rp.get());
      rt->create();
      std::unique_ptr<QRhiShaderResourceBindings> srb(rhi.newShaderResourceBindings());
      srb->setBindings({});
      srb->create();

      std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi.newGraphicsPipeline());
      pipeline->setShaderStages(
          {{QRhiShaderStage::Vertex, shaders.first},
           {QRhiShaderStage::Fragment, shaders.second}});
      pipeline->setShaderResourceBindings(srb.get());
      pipeline->setRenderPassDescriptor(rp.get());

      // The geometry's full binding set, as a mesh would publish it.
      QRhiVertexInputLayout seed;
      seed.setBindings(
          {{12, QRhiVertexInputBinding::PerVertex},
           {16, QRhiVertexInputBinding::PerVertex},
           {16, QRhiVertexInputBinding::PerVertex}});
      seed.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});
      pipeline->setVertexInputLayout(seed);

      const auto geom = makeGeometryWithInteriorOrphan();
      FallbackBindingPlan plan;
      const bool remapped
          = remapPipelineVertexInputs(*pipeline, shaders.first, geom, &plan);

      const auto& out = pipeline->vertexInputLayout();
      const int nb = bindingCount(out);

      std::vector<bool> used(nb > 0 ? nb : 0, false);
      bool inRange = true;
      for(auto it = out.cbeginAttributes(); it != out.cendAttributes(); ++it)
      {
        if(it->binding() < 0 || it->binding() >= nb)
        {
          inRange = false;
          continue;
        }
        used[it->binding()] = true;
      }
      bool allUsed = inRange;
      for(bool u : used)
        allUsed = allUsed && u;

      int posBinding = -1, colBinding = -1;
      for(auto it = out.cbeginAttributes(); it != out.cendAttributes(); ++it)
      {
        if(it->location() == 0)
          posBinding = it->binding();
        if(it->location() == 1)
          colBinding = it->binding();
      }

      std::vector<quint32> strides;
      for(auto it = out.cbeginBindings(); it != out.cendBindings(); ++it)
        strides.push_back(it->stride());

      const bool created = pipeline->create();

      CHECK(remapped);
      // Binding 1 is gone; the two the shader reads survive.
      CHECK(nb == 2);
      // No survivor is an orphan — this is the property Metal enforces.
      CHECK(allUsed);
      // Renumbered, not just removed: colour moved from binding 2 to 1 and
      // kept its shader location.
      CHECK(posBinding == 0);
      CHECK(colBinding == 1);
      // Strides follow the streams they came from, so the draw binds the right
      // buffers: the second survivor is the geometry's binding 2, not the
      // orphaned binding 1.
      REQUIRE(strides.size() == 2);
      CHECK(strides[0] == 12u);
      CHECK(strides[1] == 16u);
      // The plan is what lets the draw bind the same subset in the same order.
      CHECK(plan.compacted);
      REQUIRE(plan.mesh_bindings.size() == 2);
      CHECK(plan.mesh_bindings[0] == 0);
      CHECK(plan.mesh_bindings[1] == 2);
      // The point of all of it: this is the call that asserted on Metal.
      CHECK(created);
      ran = true;
    }
    state->destroy();
  });

  INFO(error);
  CHECK(ran);
}
