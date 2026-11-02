#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QVector3D>

#include <cstring>

namespace score::gfx
{
//! What a frame of a Model Display shows its scene with
struct ModelSceneView
{
  //! The model matrix of the node, applied over the transforms of the scene
  QMatrix4x4 model;
  QMatrix4x4 view;
  QVector3D eye;
  //! Raised whenever one of the above changed
  bool changed{};
};

/**
 * Draws a scene in full in place of the first geometry the Model Display
 * renderer draws on its own. One per renderer, on the render thread.
 */
class ModelSceneDrawer
{
public:
  virtual ~ModelSceneDrawer() = default;

  //! The scene the renderer received; returns whether it is another one
  virtual bool setScene(const ossia::scene_spec& scene) = 0;
  //! Whether it draws the scene rather than the renderer its first geometry
  virtual bool active() const noexcept = 0;
  //! The bounds of what is drawn, in the space of the scene
  virtual const ossia::aabb& bounds() const noexcept = 0;

  //! Once the passes of the renderer exist for the frame
  virtual void update(
      RenderList& renderer, QRhiResourceUpdateBatch& res, PassMap& passes,
      QRhiBuffer* processUBO, QRhiBuffer* cameraUBO, const ModelSceneView& view)
      = 0;
  virtual void draw(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) = 0;

  //! Whether there are lines to draw with the guides
  virtual bool hasLines() const noexcept = 0;
  //! With the line pipeline of the guides: world-space positions and colours
  virtual void drawLines(
      RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge,
      QRhiGraphicsPipeline* lines, QRhiShaderResourceBindings* srb)
      = 0;

  //! The GPU resources go away, the scene stays: the next update uploads it
  //! again
  virtual void release() = 0;
};

//! The bounds of the positions of a geometry held in memory; empty when they
//! are not
inline ossia::aabb geometryPositionBounds(const ossia::geometry& g) noexcept
{
  ossia::aabb box{{1.f, 1.f, 1.f}, {-1.f, -1.f, -1.f}};
  using attribute = ossia::geometry::attribute;
  const attribute* attr{};
  for(const auto& a : g.attributes)
    if(a.semantic == ossia::attribute_semantic::position)
    {
      attr = &a;
      break;
    }
  if(!attr)
    for(const auto& a : g.attributes)
      if(a.location == 0)
      {
        attr = &a;
        break;
      }
  if(!attr || (attr->format != attribute::float3 && attr->format != attribute::float4)
     || attr->binding < 0 || attr->binding >= int(g.input.size())
     || attr->binding >= int(g.bindings.size()))
    return box;
  const auto& in = g.input[attr->binding];
  if(in.buffer < 0 || in.buffer >= int(g.buffers.size()))
    return box;
  const auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[in.buffer].data);
  if(!cpu || !cpu->raw_data)
    return box;
  const int64_t element = attr->format == attribute::float3 ? 12 : 16;
  const int64_t stride = g.bindings[attr->binding].byte_stride > 0
                             ? g.bindings[attr->binding].byte_stride
                             : element;
  const int64_t first = in.byte_offset + attr->byte_offset;
  const auto* bytes = static_cast<const char*>(cpu->raw_data.get());
  for(int64_t i = 0; i < g.vertices; i++)
  {
    const int64_t at = first + i * stride;
    if(at < 0 || at + 12 > cpu->byte_size)
      break;
    float p[3];
    std::memcpy(p, bytes + at, sizeof(p));
    box.expand(p[0], p[1], p[2]);
  }
  return box;
}
}
