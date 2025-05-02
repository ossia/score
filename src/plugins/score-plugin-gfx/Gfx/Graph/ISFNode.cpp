#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/RenderedComputeNode.hpp>
#include <Gfx/Graph/RenderedISFNode.hpp>

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

  void add_texture_imgrect()
  {
    // Also add the vec4 imgRect uniform:
    while(sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }

    *reinterpret_cast<float*>(data + 0) = 0.f;
    *reinterpret_cast<float*>(data + 4) = 0.f;
    *reinterpret_cast<float*>(data + 8) = 640.f;
    *reinterpret_cast<float*>(data + 12) = 480.f;
    data += 4 * 4;
    sz += 4 * 4;
  }

  void operator()(const isf::image_input& in) noexcept
  {
    self.input.push_back(new Port{&self, {}, Types::Image, {}});
    add_texture_imgrect();
  }

  void operator()(const isf::audio_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
    add_texture_imgrect();
    data.rectUniformOffset = this->sz - 4 * 4;
  }

  void operator()(const isf::audioFFT_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    data.fft = true;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
    add_texture_imgrect();
    data.rectUniformOffset = this->sz - 4 * 4;
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

  // Size of the pass textures (vec4)
  for(std::size_t i = 0; i < desc.pass_targets.size(); i++)
    sz_vis(isf::color_input{});

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

  // Handle the pass textures size uniforms
  {
    char* data = visitor.data;
    int sz = visitor.sz;
    for(std::size_t i = 0; i < desc.pass_targets.size(); i++)
    {
      // Passes also need an _imgRect uniform
      while(sz % 16 != 0)
      {
        sz += 4;
        data += 4;
      }

      *reinterpret_cast<float*>(data + 0) = 0.f;
      *reinterpret_cast<float*>(data + 4) = 0.f;
      *reinterpret_cast<float*>(data + 8) = 640.f;
      *reinterpret_cast<float*>(data + 12) = 480.f;
      data += 4 * 4;
      sz += 4 * 4;
    }
  }

  output.push_back(new Port{this, {}, Types::Image, {}});
}

ISFNode::~ISFNode() { }

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
#if 0
  if(this->m_descriptor.compute)
  {
    [[unlikely]];
    switch(this->m_descriptor.passes.size())
    {
      case 0:
        return nullptr;
      case 1:
        return new SimpleRenderedComputeNode{*this};
        break;
      default: {
        return new RenderedComputeNode{*this};
        break;
      }
    }
  }
  else
#endif
  {
    switch(this->m_descriptor.passes.size())
    {
      case 0:
        return nullptr;
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
  }
}
}
