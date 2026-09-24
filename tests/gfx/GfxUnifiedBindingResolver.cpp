// remapPipelineVertexInputs used to exist three times over: a strict
// keep-bindings overload, a never-called twin of it that also read SEMANTIC
// off an isf::descriptor, and the fallback-aware one that services
// "REQUIRED": false. They are now one body behind two entry points, and this
// pins the parts of its contract the strict entry point did not have before.
//
// The load-bearing one is that the resolver OWNS the plan it is handed: it
// clears it on entry. The strict overload used to leave the caller's
// FallbackBindingPlan::slots alone, so a pipeline rebuilt over a plan that had
// carried a fallback slot kept that slot -- and the draw path binds each
// slot's buffer at its `binding_index`, an index the freshly built layout need
// not have.
//
// Registration:
//   score_add_gfx_test(unified_binding_resolver GfxUnifiedBindingResolver.cpp)
//
//   DISPLAY=:0 SCORE_TESTS_NO_XVFB=1 SCORE_TEST_API=vulkan \
//     ctest -R gfx_unified_binding_resolver
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>
#include <Gfx/Graph/VertexFallbackPool.hpp>

#include <isf.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// position at binding 0, colour at binding 2. Binding 1 is published by the
// geometry and read by nothing, so the resolver compacts it away and the plan
// reads {0, 2}.
ossia::geometry makeGeometry(int colourBinding)
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
  col.binding = colourBinding;
  col.location = 1;
  col.format = ossia::geometry::attribute::float4;
  col.byte_offset = 0;
  col.semantic = ossia::attribute_semantic::color0;
  geom.attributes.push_back(col);

  return geom;
}

// A plan left over from a previous build of the same pipeline: one fallback
// slot at a binding index, and a compacted mesh order.
score::gfx::FallbackBindingPlan stalePlan()
{
  score::gfx::FallbackBindingPlan p;
  p.slots.push_back({7, reinterpret_cast<QRhiBuffer*>(0x1)});
  p.mesh_bindings = {1};
  p.compacted = true;
  return p;
}

int bindingCount(const QRhiVertexInputLayout& l)
{
  return int(std::distance(l.cbeginBindings(), l.cendBindings()));
}

std::vector<std::pair<int, int>> attrBindings(const QRhiVertexInputLayout& l)
{
  std::vector<std::pair<int, int>> v;
  for(auto it = l.cbeginAttributes(); it != l.cendAttributes(); ++it)
    v.emplace_back(it->location(), it->binding());
  return v;
}

std::vector<quint32> strides(const QRhiVertexInputLayout& l)
{
  std::vector<quint32> v;
  for(auto it = l.cbeginBindings(); it != l.cendBindings(); ++it)
    v.push_back(it->stride());
  return v;
}
}

TEST_CASE("the vertex-input resolver owns the plan it is handed", "[gfx][gpu]")
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

      // A vertex stage with no `in` variables at all: the resolver returns
      // early, and must still have taken the caller's plan over.
      const auto inputless = makeShaders(*state, QStringLiteral(R"_(#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
)_"),
          QStringLiteral(R"_(#version 450
layout(location=0) out vec4 frag;
void main() { frag = vec4(1.0); }
)_"));

      std::unique_ptr<QRhiShaderResourceBindings> srb(rhi.newShaderResourceBindings());
      srb->setBindings({});
      srb->create();

      // The geometry's full binding set, as a mesh would publish it.
      QRhiVertexInputLayout seed;
      seed.setBindings(
          {{12, QRhiVertexInputBinding::PerVertex},
           {16, QRhiVertexInputBinding::PerVertex},
           {16, QRhiVertexInputBinding::PerVertex}});
      seed.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});

      const auto newPipeline = [&] {
        auto* p = rhi.newGraphicsPipeline();
        p->setShaderResourceBindings(srb.get());
        p->setVertexInputLayout(seed);
        return std::unique_ptr<QRhiGraphicsPipeline>(p);
      };

      const auto geom = makeGeometry(/*colourBinding=*/2);

      // 1. A stale fallback slot must not survive into this build's plan.
      auto pip1 = newPipeline();
      auto plan1 = stalePlan();
      const bool remapped1
          = remapPipelineVertexInputs(*pip1, shaders.first, geom, &plan1);

      // 2. Same, through the early return an input-less vertex stage takes.
      auto pip2 = newPipeline();
      auto plan2 = stalePlan();
      const bool remapped2
          = remapPipelineVertexInputs(*pip2, inputless.first, geom, &plan2);

      // 3. Both entry points are the same resolver: with a descriptor that
      //    declares nothing, the fallback-aware one must land on exactly the
      //    layout and plan the strict one produces.
      VertexFallbackPool pool;
      auto* batch = rhi.nextResourceUpdateBatch();
      isf::descriptor empty_desc;
      auto pip3 = newPipeline();
      FallbackBindingPlan plan3;
      const bool remapped3 = remapPipelineVertexInputs(
          *pip3, shaders.first, geom, empty_desc, rhi, pool, *batch, plan3);
      batch->release();

      // 4. An attribute whose binding is outside the mesh's layout: the
      //    strict path refuses the pipeline, as the fallback-aware one does.
      auto pip4 = newPipeline();
      FallbackBindingPlan plan4;
      const bool remapped4 = remapPipelineVertexInputs(
          *pip4, shaders.first, makeGeometry(/*colourBinding=*/5), &plan4);

      CHECK(remapped1);
      // THE pin: the resolver clears the plan on entry.
      CHECK(plan1.slots.empty());
      CHECK(plan1.compacted);
      REQUIRE(plan1.mesh_bindings.size() == 2);
      CHECK(plan1.mesh_bindings[0] == 0);
      CHECK(plan1.mesh_bindings[1] == 2);

      CHECK(remapped2);
      CHECK(plan2.slots.empty());
      CHECK(plan2.mesh_bindings.empty());
      CHECK(!plan2.compacted);

      CHECK(remapped3);
      CHECK(bindingCount(pip3->vertexInputLayout()) == 2);
      CHECK(strides(pip3->vertexInputLayout()) == strides(pip1->vertexInputLayout()));
      CHECK(attrBindings(pip3->vertexInputLayout())
            == attrBindings(pip1->vertexInputLayout()));
      CHECK(plan3.mesh_bindings == plan1.mesh_bindings);
      CHECK(plan3.slots.empty());

      CHECK(!remapped4);

      ran = true;
    }
    state->destroy();
  });

  INFO(error);
  CHECK(ran);
}
