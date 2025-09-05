#include <Gfx/Graph/GeometryFilterNode.hpp>
#include <Gfx/Graph/GeometryFilterNodeRenderer.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <score/tools/Debug.hpp>

#include <boost/algorithm/string/replace.hpp>

namespace score::gfx
{
struct geometry_input_port_vis
{
  GeometryFilterNode& self;
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

  void operator()(const isf::image_input& in) noexcept
  {
    // FIXME
    // self.input.push_back(new Port{&self, {}, Types::Image, {}});
    // add_texture_imgrect();
  }

  void operator()(const isf::audio_input& audio) noexcept
  {
    // FIXME
    // self.m_audio_textures.push_back({});
    // auto& data = self.m_audio_textures.back();
    // data.fixedSize = audio.max;
    // self.input.push_back(new Port{&self, &data, Types::Audio, {}});
    // add_texture_imgrect();
    // data.rectUniformOffset = this->sz - 4 * 4;
  }

  void operator()(const isf::audioFFT_input& audio) noexcept
  {
    // self.m_audio_textures.push_back({});
    // auto& data = self.m_audio_textures.back();
    // data.fixedSize = audio.max;
    // data.fft = true;
    // self.input.push_back(new Port{&self, &data, Types::Audio, {}});
    // add_texture_imgrect();
    // data.rectUniformOffset = this->sz - 4 * 4;
  }
  void operator()(const isf::audioHist_input& audio) noexcept { }
};

GeometryFilterNode::GeometryFilterNode(const isf::descriptor& desc, const QString& vert)
    : m_descriptor{desc}
{
  input.push_back(new Port{this, {}, Types::Geometry, {}});
  output.push_back(new Port{this, {}, Types::Geometry, {}});
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
  geometry_input_port_vis visitor{*this, cur};
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
      *reinterpret_cast<float*>(data + 8) = 640.f; // FIXME
      *reinterpret_cast<float*>(data + 12) = 480.f;
      data += 4 * 4;
      sz += 4 * 4;
    }
  }
}

GeometryFilterNode::~GeometryFilterNode() { }

score::gfx::NodeRenderer*
GeometryFilterNode::createRenderer(RenderList& r) const noexcept
{
  return new GeometryFilterNodeRenderer{*this};
}
}
