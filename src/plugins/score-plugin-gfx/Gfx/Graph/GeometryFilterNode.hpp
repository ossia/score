#pragma once
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <isf.hpp>

namespace score::gfx
{
struct GeometryFilterNodeRenderer;
struct RenderedGeometryFilterNode;
struct geometry_input_port_vis;
/**
 * @brief Data model for geometry filters.
 */
class SCORE_PLUGIN_GFX_EXPORT GeometryFilterNode : public score::gfx::ProcessNode
{
public:
  GeometryFilterNode(const isf::descriptor& desc, const QString& filter);

  virtual ~GeometryFilterNode();
  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  const isf::descriptor& descriptor() const noexcept { return m_descriptor; }

  friend GeometryFilterNodeRenderer;
  friend geometry_input_port_vis;

  isf::descriptor m_descriptor;
  std::unique_ptr<char[]> m_material_data;
  int m_materialSize{};
};
}
