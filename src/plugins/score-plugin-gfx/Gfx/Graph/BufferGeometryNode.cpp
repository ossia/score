#include <Gfx/Graph/BufferGeometryNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/small_flat_map.hpp>

#include <set>

namespace score::gfx
{

/**
 * @brief Renderer for BufferGeometryNode
 */
class BufferGeometryRenderer : public score::gfx::NodeRenderer
{
public:
  explicit BufferGeometryRenderer(const BufferGeometryNode& node) noexcept
    : NodeRenderer{static_cast<const Node&>(node)}
    , m_node{node}
  {
  }

  TextureRenderTarget renderTargetForInput(const Port& p) override
  {
    auto it = m_rts.find(&p);
    if(it != m_rts.end())
      return it->second;
    return {};
  }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    // Create render targets for texture inputs
    // BufferGeometry takes up to 4 texture buffers as input
    int tex_idx = 0;
    for(auto in : m_node.input)
    {
      if(tex_idx >= 4)
        break;
        
      // Create a storage texture render target for buffer inputs
      const QSize default_size{1024, 1}; // 1D buffer texture
      auto rt = score::gfx::createRenderTarget(
          renderer.state, 
          QRhiTexture::RGBA32F, 
          default_size, 
          renderer.samples(), 
          false); // No depth buffer needed for compute buffers
      
      if(rt.texture)
      {
        rt.texture->setName(
            QByteArray("BufferGeometryNode::input_") + QByteArray::number(tex_idx));
      }
      if(rt.renderTarget)
      {
        rt.renderTarget->setName(
            QByteArray("BufferGeometryNode::rt_") + QByteArray::number(tex_idx));
      }
      
      m_rts[in] = std::move(rt);
      tex_idx++;
    }
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    for(int i = 0; i < 4; i++)
    {
      auto port_i = node.input[i]->value;
      qDebug() << port_i << static_cast<QRhiTexture*>(port_i);
    }
    // Update geometry specification if needed
    if(m_node.m_geometry_dirty)
    {
      m_node.updateGeometrySpec();
    }
  }

  void release(RenderList&) override
  {
    // Release render targets
    for(auto& [port, rt] : m_rts)
    {
      rt.release();
    }
    m_rts.clear();
  }

private:
  const BufferGeometryNode& m_node;
  ossia::small_flat_map<const Port*, TextureRenderTarget, 4> m_rts;
};

BufferGeometryNode::BufferGeometryNode()
{
  // Set up default configuration
  config.position.location = 0;
  config.position.format = ossia::geometry::attribute::fp3;
  
  config.normal.location = 1;
  config.normal.format = ossia::geometry::attribute::fp3;
  
  config.texcoord0.location = 2;
  config.texcoord0.format = ossia::geometry::attribute::fp2;
  
  config.color.location = 4;
  config.color.format = ossia::geometry::attribute::fp4;
  
  // Create default mesh and filter lists
  m_meshes = std::make_shared<ossia::mesh_list>();
  m_filters = std::make_shared<ossia::geometry_filter_list>();
  m_geometry_spec = {m_meshes, m_filters};

  for(int i = 0; i < 4; i++)
  {
    input.push_back(new Port{static_cast<Node*>(this), {}, Types::Image, {}});
  }
  // vertex count
  input.push_back(new Port{static_cast<Node*>(this), {}, Types::Int, {}});

  // position
  // -> location
  // -> format
  // -> offset
  // -> stride
  // -> buffer
  for(int i = 0; i < 5 * 5; i++)
    input.push_back(new Port{static_cast<Node*>(this), {}, Types::Int, {}});

  // Create output port for geometry
  output.push_back(new Port{static_cast<Node*>(this), {}, Types::Geometry, {}});
}

BufferGeometryNode::~BufferGeometryNode() = default;

score::gfx::NodeRenderer* BufferGeometryNode::createRenderer(RenderList& r) const noexcept
{
  return new BufferGeometryRenderer{*this};
}

void BufferGeometryNode::process(Message&& msg)
{
  // Handle incoming messages to update configuration
  // For now, geometry updates will be handled via the configuration directly
  
  // Mark geometry as dirty when buffers change
  if(!msg.input.empty())
  {
    m_geometry_dirty = true;
  }
  
  // Update output with current geometry spec  
  if(m_geometry_dirty)
  {
    updateGeometrySpec();
    
    // Send geometry spec to output
    if(!output.empty() && output[0]->value)
    {
      ossia::geometry_port geom_port;
      geom_port.geometry = m_geometry_spec;
      geom_port.flags = ossia::geometry_port::dirty_meshes;
      
      // Store in output port value
      *static_cast<ossia::geometry_port*>(output[0]->value) = geom_port;
    }
  }
}

void BufferGeometryNode::updateGeometrySpec() const
{
  if(!m_geometry_dirty)
    return;
    
  // Clear existing meshes
  m_meshes->meshes.clear();
  
  // Create a single geometry from the configuration
  ossia::geometry geometry;
  
  // Set up buffers - reference the input texture buffers
  geometry.buffers.clear();
  for(std::size_t i = 0; i < input.size() && i < 8; ++i)
  {
    if(input[i]->type == Types::Image && input[i]->value)
    {
      // Create buffer with proper initialization
      geometry.buffers.push_back(
          ossia::geometry::buffer{.data = ossia::geometry::gpu_buffer{}, .dirty = true});
    }
  }
  
  // Set up bindings based on unique strides
  geometry.bindings.clear();
  std::set<uint32_t> unique_strides;
  
  auto add_binding_if_needed = [&](uint32_t stride) -> int {
    if(stride == 0) return -1;
    
    auto it = unique_strides.find(stride);
    if(it == unique_strides.end())
    {
      unique_strides.insert(stride);
      ossia::geometry::binding binding;
      binding.stride = stride;
      binding.classification = ossia::geometry::binding::per_vertex;
      binding.step_rate = 1;
      
      geometry.bindings.push_back(binding);
      return static_cast<int>(geometry.bindings.size() - 1);
    }
    else
    {
      return static_cast<int>(std::distance(unique_strides.begin(), it));
    }
  };
  
  // Set up attributes
  geometry.attributes.clear();
  
  auto add_attribute = [&](const AttributeMapping& mapping, int binding_idx) {
    if(!mapping.enabled() || binding_idx < 0) return;
    if(mapping.buffer_index >= static_cast<int>(geometry.buffers.size())) return;
    
    ossia::geometry::attribute attr;
    attr.binding = binding_idx;
    attr.location = mapping.location;
    // Cast int to enum properly
    switch(mapping.format) {
      case 0: attr.format = ossia::geometry::attribute::fp1; break;
      case 1: attr.format = ossia::geometry::attribute::fp2; break;
      case 2: attr.format = ossia::geometry::attribute::fp3; break;
      case 3: attr.format = ossia::geometry::attribute::fp4; break;
      case 4: attr.format = ossia::geometry::attribute::unsigned1; break;
      case 5: attr.format = ossia::geometry::attribute::unsigned2; break;
      case 6: attr.format = ossia::geometry::attribute::unsigned4; break;
      default: attr.format = ossia::geometry::attribute::fp3; break;
    }
    attr.offset = mapping.offset;
    
    geometry.attributes.push_back(attr);
  };
  
  // Add standard attributes
  int pos_binding = add_binding_if_needed(config.position.stride);
  int norm_binding = add_binding_if_needed(config.normal.stride);
  int tex0_binding = add_binding_if_needed(config.texcoord0.stride);
  int tex1_binding = add_binding_if_needed(config.texcoord1.stride);
  int color_binding = add_binding_if_needed(config.color.stride);
  int tangent_binding = add_binding_if_needed(config.tangent.stride);
  int bitangent_binding = add_binding_if_needed(config.bitangent.stride);
  
  add_attribute(config.position, pos_binding);
  add_attribute(config.normal, norm_binding);
  add_attribute(config.texcoord0, tex0_binding);
  add_attribute(config.texcoord1, tex1_binding);
  add_attribute(config.color, color_binding);
  add_attribute(config.tangent, tangent_binding);
  add_attribute(config.bitangent, bitangent_binding);
  
  // Add custom attributes
  for(const auto& custom : config.custom)
  {
    int custom_binding = add_binding_if_needed(custom.stride);
    add_attribute(custom, custom_binding);
  }
  
  // Set up input references
  geometry.input.clear();
  for(std::size_t i = 0; i < geometry.buffers.size(); ++i)
  {
    struct ossia::geometry::input inp;
    inp.buffer = static_cast<int>(i);
    inp.offset = 0;
    geometry.input.push_back(inp);
  }
  
  // Set geometry properties with proper enum casts
  geometry.vertices = config.vertex_count;
  geometry.indices = config.index_buffer.enabled ? config.index_buffer.count : 0;
  
  switch(config.topology) {
    case 0: geometry.topology = ossia::geometry::triangles; break;
    case 1: geometry.topology = ossia::geometry::triangle_strip; break;
    case 2: geometry.topology = ossia::geometry::triangle_fan; break;
    case 3: geometry.topology = ossia::geometry::lines; break;
    case 4: geometry.topology = ossia::geometry::line_strip; break;
    case 5: geometry.topology = ossia::geometry::points; break;
    default: geometry.topology = ossia::geometry::triangles; break;
  }
  
  switch(config.cull_mode) {
    case 0: geometry.cull_mode = ossia::geometry::none; break;
    case 1: geometry.cull_mode = ossia::geometry::front; break;
    case 2: geometry.cull_mode = ossia::geometry::back; break;
    default: geometry.cull_mode = ossia::geometry::back; break;
  }
  
  switch(config.front_face) {
    case 0: geometry.front_face = ossia::geometry::counter_clockwise; break;
    case 1: geometry.front_face = ossia::geometry::clockwise; break;
    default: geometry.front_face = ossia::geometry::counter_clockwise; break;
  }
  
  // Set up index buffer if enabled
  if(config.index_buffer.enabled)
  {
    geometry.index.buffer = config.index_buffer.buffer_index;
    geometry.index.offset = config.index_buffer.offset;

    using idx_fmt_t = std::remove_reference_t<decltype(ossia::geometry{}.index.format)>;
    switch(config.index_buffer.format) {
      case 0:
        geometry.index.format = idx_fmt_t::uint16;
        break;
      case 1:
        geometry.index.format = idx_fmt_t::uint32;
        break;
      default:
        geometry.index.format = idx_fmt_t::uint16;
        break;
    }
  }
  else
  {
    geometry.index.buffer = -1;
  }
  
  // Add geometry to mesh list
  m_meshes->meshes.clear();
  m_meshes->meshes.push_back(std::move(geometry));
  m_meshes->dirty_index++;
  
  // Update filters list (empty for now)
  m_filters->filters.clear();
  m_filters->dirty_index++;
  
  m_geometry_dirty = false;
}

}
