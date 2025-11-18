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
  GeometryFilterNode(int64_t index, const isf::descriptor& desc, QString filter);

  virtual ~GeometryFilterNode();
  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  const isf::descriptor& descriptor() const noexcept { return m_descriptor; }

  friend GeometryFilterNodeRenderer;
  friend geometry_input_port_vis;

  int64_t m_index{};

  std::string m_shader;
  isf::descriptor m_descriptor;
  std::unique_ptr<char[]> m_material_data;
  int m_materialSize{};

  alignas(8) ossia::transform3d m_transform;
  int m_dirtyTransformIndex{};

  void process(Message&& msg) override;
  // void process(int32_t port, const ossia::geometry_spec& v) override;
  void process(int32_t port, const ossia::transform3d& v) override;
};
}
