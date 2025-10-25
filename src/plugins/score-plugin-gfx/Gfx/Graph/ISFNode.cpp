#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFNode.hpp>
#include <Gfx/Graph/RenderedVSANode.hpp>
#include <Gfx/Graph/SimpleRenderedISFNode.hpp>

#include <score/tools/Debug.hpp>

#include <boost/algorithm/string/replace.hpp>

namespace score::gfx
{
struct isf_input_port_vis
{
  ISFNode& self;
  char* data{};
  int sz{};

  void operator()(const isf::float_input& in) noexcept
  {
    if(in.def != 0.)
      *reinterpret_cast<float*>(data) = in.def;
    else
      *reinterpret_cast<float*>(data) = (in.max - in.min) / 2.;
    self.input.push_back(new Port{&self, data, Types::Float, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::long_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = in.def;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::event_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = 0;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    self.m_event_ports.push_back(reinterpret_cast<int*>(data));
    data += 4;
    sz += 4;
  }

  void operator()(const isf::bool_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = in.def;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::point2d_input& in) noexcept
  {
    if(sz % 8 != 0)
    {
      sz += 4;
      data += 4;
    }

    const auto& arr = in.def.value_or(std::array<double, 2>{0.5, 0.5});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    self.input.push_back(new Port{&self, data, Types::Vec2, {}});
    data += 2 * 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input& in) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }
    const auto& arr = in.def.value_or(std::array<double, 3>{0.5, 0.5, 0.5});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    *reinterpret_cast<float*>(data + 8) = arr[2];
    self.input.push_back(new Port{&self, data, Types::Vec3, {}});
    data += 3 * 4;
    sz += 3 * 4;
  }

  void operator()(const isf::color_input& in) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }
    const auto& arr = in.def.value_or(std::array<double, 4>{0.5, 0.5, 0.5, 0.5});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    *reinterpret_cast<float*>(data + 8) = arr[2];
    *reinterpret_cast<float*>(data + 12) = arr[3];
    self.input.push_back(new Port{&self, data, Types::Vec4, {}});
    data += 4 * 4;
    sz += 4 * 4;
  }

  void operator()(const isf::image_input& in) noexcept
  {
    self.input.push_back(new Port{&self, {}, Types::Image, {}});
  }

  void operator()(const isf::audio_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  void operator()(const isf::audioHist_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    data.mode = data.Histogram;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  void operator()(const isf::audioFFT_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    data.mode = AudioTexture::Mode::FFT;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  // CSF-specific input handlers
  void operator()(const isf::storage_input& in) noexcept
  {
    // According to the spec:
    // - read_only: input port
    // - write_only: output port
    // - read_write: output port only, buffer is persistent

    if(in.access == "read_only")
    {
      // Create input port for read-only storage buffer
      self.input.push_back(new Port{&self, {}, Types::Buffer, {}});
    }
    else if(in.access.contains("write"))
    {
      // Create output port for write-only storage buffer
      self.output.push_back(new Port{&self, {}, Types::Image, {}});

      // Check for flexible array member
      if(!in.layout.empty())
      {
        const auto& lastField = in.layout.back();
        if(lastField.type.find("[]") != std::string::npos)
        {
          (*this)(isf::long_input{.values = {}, .labels = {}, .def = 1024});
        }
      }
    }
  }

  void operator()(const isf::texture_input& in) noexcept
  {
    // Texture inputs are always input ports (sampled textures)
    self.input.push_back(new Port{&self, {}, Types::Image, {}});
  }

  void operator()(const isf::csf_image_input& in) noexcept
  {
    // CSF image inputs - these can be read-only, write-only, or read-write
    if(in.access == "read_only")
    {
      // Input port for read-only image
      self.input.push_back(new Port{&self, {}, Types::Image, {}});
    }
    else if(in.access == "write_only" || in.access == "read_write")
    {
      // Output port for the image
      self.output.push_back(new Port{&self, {}, Types::Image, {}});
      // FIXME:
      // (*this)(isf::point2d_input{ ... });
      // (*this)(isf::long_input{ ... });
    }
  }
};

ISFNode::ISFNode(const isf::descriptor& desc, const QString& vert, const QString& frag)
    : m_descriptor{desc}
{
  m_vertexS = vert;
  m_fragmentS = frag;

  // Compoute the size required for the materials
  isf_input_size_vis sz_vis{};

  // Size of the inputs
  for(const isf::input& input : desc.inputs)
  {
    ossia::visit(sz_vis, input.data);
  }

  m_materialSize = sz_vis.sz;

  // Allocate the required memory
  // TODO : this must be per-renderer, as the texture sizes may depend on the renderer....
  m_material_data.reset(new char[m_materialSize]);
  std::fill_n(m_material_data.get(), m_materialSize, 0);
  char* cur = m_material_data.get();

  // Create ports pointing to the data used for the UBO
  isf_input_port_vis visitor{*this, cur};
  for(const isf::input& input : desc.inputs)
    ossia::visit(visitor, input.data);

  output.push_back(new Port{this, {}, Types::Image, {}});
}

ISFNode::ISFNode(const isf::descriptor& desc, const QString& comp)
    : m_descriptor{desc}
{
  m_computeS = comp;

  // Compoute the size required for the materials
  isf_input_size_vis sz_vis{};

  // Size of the inputs
  for(const isf::input& input : desc.inputs)
  {
    ossia::visit(sz_vis, input.data);
  }

  m_materialSize = sz_vis.sz;

  // Allocate the required memory
  // TODO : this must be per-renderer, as the texture sizes may depend on the renderer....
  m_material_data.reset(new char[m_materialSize]);
  std::fill_n(m_material_data.get(), m_materialSize, 0);
  char* cur = m_material_data.get();

  // Create ports pointing to the data used for the UBO
  isf_input_port_vis visitor{*this, cur};
  for(const isf::input& input : desc.inputs)
    ossia::visit(visitor, input.data);

  // For CSF, if no output ports were created by the inputs, create a default one
  if(desc.mode == isf::descriptor::CSF && output.empty())
  {
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
}

ISFNode::~ISFNode() { }

void ISFNode::process(Message&& msg)
{
  // Clear potential event ports
  for(int* event : this->m_event_ports)
    *event = 0;

  // Normal processing
  ProcessNode::process(std::move(msg));
}

QSize ISFNode::computeTextureSize(const isf::pass& pass, QSize origSize)
{
  QSize res = origSize;

  ossia::math_expression e;
  ossia::small_pod_vector<double, 16> data;

  // Note : reserve is super important here,
  // as the expression parser takes *references* to the
  // variables.
  data.reserve(2 + m_descriptor.inputs.size());

  e.add_constant("var_WIDTH", data.emplace_back(res.width()));
  e.add_constant("var_HEIGHT", data.emplace_back(res.height()));
  int port_k = 0;
  for(const isf::input& input : m_descriptor.inputs)
  {
    auto port = this->input[port_k];
    if(ossia::get_if<isf::float_input>(&input.data))
    {
      e.add_constant("var_" + input.name, data.emplace_back(*(float*)port->value));
    }
    else if(ossia::get_if<isf::long_input>(&input.data))
    {
      e.add_constant("var_" + input.name, data.emplace_back(*(int*)port->value));
    }

    port_k++;
  }

  if(auto expr = pass.width_expression; !expr.empty())
  {
    boost::algorithm::replace_all(expr, "$", "var_");
    e.register_symbol_table();
    bool ok = e.set_expression(expr);
    if(ok)
      res.setWidth(e.value());
    else
      qDebug() << e.error().c_str() << expr.c_str();
  }
  if(auto expr = pass.height_expression; !expr.empty())
  {
    boost::algorithm::replace_all(expr, "$", "var_");
    e.register_symbol_table();
    bool ok = e.set_expression(expr);
    if(ok)
      res.setHeight(e.value());
    else
      qDebug() << e.error().c_str() << expr.c_str();
  }

  return res;
}

score::gfx::NodeRenderer* ISFNode::createRenderer(RenderList& r) const noexcept
{
  if(this->m_descriptor.passes.empty() && this->m_descriptor.csf_passes.empty())
  {
    return nullptr;
  }

  switch(this->m_descriptor.mode)
  {
    case isf::descriptor::VSA: {
      return new SimpleRenderedVSANode{*this};
    }
    case isf::descriptor::CSF:
      return new RenderedCSFNode{*this};

    case isf::descriptor::ISF:
      switch(this->m_descriptor.passes.size())
      {
        case 1:
          if(!this->m_descriptor.passes[0].persistent)
          {
            return new SimpleRenderedISFNode{*this};
          }
          else
          {
            return new RenderedISFNode{*this};
          }
          break;
        default: {
          return new RenderedISFNode{*this};
          break;
        }
      }
      break;
  }
  return nullptr;
}
}
