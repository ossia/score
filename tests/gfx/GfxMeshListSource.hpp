#pragma once
// A geometry producer for CSF tests: publishes a fixed mesh list, each mesh
// with its own interleaved vertex buffer (CPU or GPU) and an optional uint32
// index buffer.
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstring>
#include <memory>
#include <vector>

namespace score::test::gfx
{

struct SourceMesh
{
  struct Attribute
  {
    ossia::attribute_semantic semantic{};
    decltype(ossia::geometry::attribute::format) format{};
    uint32_t byte_offset{};
  };

  std::vector<float> data; // interleaved, stride bytes per vertex
  int vertices{};
  uint32_t stride{};
  std::vector<Attribute> attributes;
  std::vector<uint32_t> index;
};

struct MeshListSourceNode final : score::gfx::ProcessNode
{
  std::vector<SourceMesh> meshes;
  bool gpu{};

  MeshListSourceNode(std::vector<SourceMesh> m, bool on_gpu)
      : meshes{std::move(m)}
      , gpu{on_gpu}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct MeshListSourceRenderer final : score::gfx::NodeRenderer
{
  const MeshListSourceNode& node;
  std::vector<QRhiBuffer*> owned;
  ossia::geometry_spec m_spec;

  explicit MeshListSourceRenderer(const MeshListSourceNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  ossia::geometry::buffer
  makeBuffer(QRhi& rhi, QRhiResourceUpdateBatch& res, const void* src, int64_t size)
  {
    if(!node.gpu)
    {
      std::shared_ptr<void> copy(new char[size], std::default_delete<char[]>());
      std::memcpy(copy.get(), src, size);
      return {.data = ossia::geometry::cpu_buffer{std::move(copy), size}};
    }
    auto* b = rhi.newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer | QRhiBuffer::IndexBuffer,
        size);
    b->setName("MeshListSource");
    b->create();
    res.uploadStaticBuffer(b, 0, size, src);
    owned.push_back(b);
    return {.data = ossia::geometry::gpu_buffer{b, size}};
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    for(const auto& sm : node.meshes)
    {
      ossia::geometry g;
      g.vertices = sm.vertices;
      g.topology = ossia::geometry::points;
      g.cull_mode = ossia::geometry::none;
      g.front_face = ossia::geometry::counter_clockwise;
      g.buffers.push_back(
          makeBuffer(rhi, res, sm.data.data(), int64_t(sm.data.size() * sizeof(float))));
      g.bindings.push_back({.byte_stride = sm.stride});
      g.input.push_back({.buffer = 0, .byte_offset = 0});
      int loc = 0;
      for(const auto& a : sm.attributes)
      {
        ossia::geometry::attribute attr;
        attr.binding = 0;
        attr.location = loc++;
        attr.format = a.format;
        attr.byte_offset = a.byte_offset;
        attr.semantic = a.semantic;
        g.attributes.push_back(attr);
      }
      if(!sm.index.empty())
      {
        g.buffers.push_back(makeBuffer(
            rhi, res, sm.index.data(), int64_t(sm.index.size() * sizeof(uint32_t))));
        g.index.buffer = 1;
        g.index.format = decltype(g.index)::uint32;
        g.indices = int(sm.index.size());
      }
      m_spec.meshes->meshes.push_back(std::move(g));
    }
    m_spec.meshes->dirty_index = 1;
    m_initialized = true;
  }

  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(!m_spec.meshes || !sink || !sink->node)
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
    for(auto* b : owned)
      r.releaseBuffer(b);
    owned.clear();
    m_spec = {};
    m_initialized = false;
  }
};

inline score::gfx::NodeRenderer*
MeshListSourceNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new MeshListSourceRenderer{*this};
}

}
