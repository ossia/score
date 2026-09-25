#pragma once
// A geometry producer for the A5 tests: a viewport-covering triangle and an
// `items` auxiliary of `count` vec4 published at `offset` inside one fixed
// 4096-byte GPU buffer, the way ScenePreprocessor publishes its grown
// buckets. Changing `count` or `offset` republishes the same buffer handle
// with a new range; item i holds (i + 1, 0, 0, 0).
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <algorithm>
#include <vector>

namespace a5
{
struct AuxProducerNode final : score::gfx::ProcessNode
{
  static constexpr int kCapacity = 4096;
  int count{24};
  int offset{0};

  AuxProducerNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct AuxProducerRenderer final : score::gfx::NodeRenderer
{
  const AuxProducerNode& node;
  QRhiBuffer* pos{};
  QRhiBuffer* items{};
  ossia::geometry_spec m_spec;
  int m_count{-1};
  int m_offset{-1};

  explicit AuxProducerRenderer(const AuxProducerNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  void publish()
  {
    ossia::geometry g;
    g.vertices = 3;
    g.topology = ossia::geometry::triangles;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;
    g.buffers.push_back({.data = ossia::geometry::gpu_buffer{pos, 3 * 16}});
    g.buffers.push_back(
        {.data = ossia::geometry::gpu_buffer{items, AuxProducerNode::kCapacity}});
    g.bindings.push_back({.byte_stride = 16});
    ossia::geometry::attribute a_pos;
    a_pos.binding = 0;
    a_pos.location = 0;
    a_pos.format = ossia::geometry::attribute::float4;
    a_pos.semantic = ossia::attribute_semantic::position;
    g.attributes.push_back(a_pos);
    g.input.push_back({.buffer = 0, .byte_offset = 0});
    g.auxiliary.push_back(
        {.name = "items",
         .buffer = 1,
         .byte_offset = node.offset,
         .byte_size = node.count * 16});

    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    m_spec.meshes->meshes.push_back(std::move(g));
    m_count = node.count;
    m_offset = node.offset;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    pos = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, 3 * 16);
    pos->setName("a5.pos");
    pos->create();
    items = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
        AuxProducerNode::kCapacity);
    items->setName("a5.items");
    items->create();

    const float tri[12] = {-1.f, -1.f, 0.f, 1.f, 3.f, -1.f, 0.f, 1.f, -1.f, 3.f, 0.f, 1.f};
    res.uploadStaticBuffer(pos, 0, sizeof(tri), tri);
    std::vector<float> v(AuxProducerNode::kCapacity / 4, 0.f);
    for(int i = 0; i < AuxProducerNode::kCapacity / 16; i++)
      v[i * 4] = float(i + 1);
    res.uploadStaticBuffer(items, 0, AuxProducerNode::kCapacity, v.data());
    publish();
    m_initialized = true;
  }

  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
    if(node.count != m_count || node.offset != m_offset)
      publish();
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(edge.source != node.output[0] || !m_spec.meshes || !sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn_it->second->process(int(it - sink->node->input.begin()), m_spec, edge.source);
  }

  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&)
      override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList& r) override
  {
    for(auto** b : {&pos, &items})
    {
      if(*b)
        r.releaseBuffer(*b);
      *b = nullptr;
    }
    m_spec = {};
    m_initialized = false;
  }
};

inline score::gfx::NodeRenderer*
AuxProducerNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new AuxProducerRenderer{*this};
}
}
