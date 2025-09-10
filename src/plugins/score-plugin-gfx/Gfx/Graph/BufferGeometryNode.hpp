#pragma once

#include <Gfx/Graph/Node.hpp>
#include <ossia/dataflow/geometry_port.hpp>

namespace score::gfx
{

/**
 * @brief Configuration for a single vertex attribute mapping
 */
struct AttributeMapping
{
  int buffer_index{0};      // Which input buffer to read from
  uint32_t offset{0};       // Byte offset within the buffer
  uint32_t stride{0};       // Stride between consecutive elements
  int format{ossia::geometry::attribute::fp3}; // Data format (fp3, fp4, etc.)
  int location{-1};         // Vertex shader location (-1 = disabled)
  
  bool enabled() const noexcept { return location >= 0; }
};

/**
 * @brief A node that converts buffer inputs into geometry specifications
 * 
 * This node takes multiple texture/buffer inputs and allows the user to specify
 * how data from these buffers should be interpreted as vertex attributes.
 * It produces a geometry_spec that references the input buffers with the
 * specified attribute mappings.
 */
struct BufferGeometryNode : NodeModel
{
public:
  explicit BufferGeometryNode();
  virtual ~BufferGeometryNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  /**
   * @brief Configuration structure for the geometry
   */
  struct Configuration
  {
    // Standard vertex attributes
    AttributeMapping position{};    // Usually location 0, fp3 or fp4
    AttributeMapping normal{};      // Usually location 1, fp3
    AttributeMapping texcoord0{};   // Usually location 2, fp2
    AttributeMapping texcoord1{};   // Usually location 3, fp2
    AttributeMapping color{};       // Usually location 4, fp4
    AttributeMapping tangent{};     // Usually location 5, fp3 or fp4
    AttributeMapping bitangent{};   // Usually location 6, fp3
    
    // Custom attributes (up to 8 additional)
    std::array<AttributeMapping, 8> custom{};
    
    // Geometry properties
    int vertex_count{0};            // Number of vertices
    int topology{ossia::geometry::triangles};
    int cull_mode{ossia::geometry::back};
    int front_face{ossia::geometry::counter_clockwise};
    
    // Index buffer (optional) 
    struct {
      bool enabled{false};
      int buffer_index{0};
      uint32_t offset{0};
      int format{0}; // 0 = uint16, 1 = uint32 (matches ossia::geometry)
      int count{0};
    } index_buffer;
    
  } config;

  // Cached geometry specification
  mutable std::shared_ptr<ossia::mesh_list> m_meshes;
  mutable std::shared_ptr<ossia::geometry_filter_list> m_filters;
  mutable ossia::geometry_spec m_geometry_spec;
  mutable bool m_geometry_dirty{true};
  
  void updateGeometrySpec() const;

private:
  void process(Message&& msg) override;
};

}