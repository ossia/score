#include "isfnode.hpp"

namespace
{

struct input_size_vis
{
  int sz{};
  void operator()(const isf::float_input&) noexcept { sz += 4; }

  void operator()(const isf::long_input&) noexcept { sz += 4; }

  void operator()(const isf::event_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::bool_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::point2d_input&) noexcept
  {
    if (sz % 8 != 0)
      sz += 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input&) noexcept
  {
    while (sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 3 * 4;
  }

  void operator()(const isf::color_input&) noexcept
  {
    while (sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 4 * 4;
  }

  void operator()(const isf::image_input&) noexcept { }

  void operator()(const isf::audio_input&) noexcept { }

  void operator()(const isf::audioFFT_input&) noexcept { }
};

struct input_port_vis
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
    if (sz % 8 != 0)
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
    while (sz % 16 != 0)
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
    while (sz % 16 != 0)
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
    self.audio_textures.push_back({});
    auto& data = self.audio_textures.back();
    data.fixedSize = audio.max;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  void operator()(const isf::audioFFT_input&) noexcept { }
};

}

ISFNode::ISFNode(const isf::descriptor& desc, const QShader& vert, const QShader& frag)
    : ISFNode{desc, vert, frag, &TexturedTriangle::instance()}
{
}

ISFNode::ISFNode(const isf::descriptor& desc, const QShader& vert, const QShader& frag, const Mesh* mesh)
    : m_mesh{mesh}
{
  setShaders(vert, frag);

  int i = 0;
  input_size_vis sz_vis{};
  for (const isf::input& input : desc.inputs)
  {
    std::visit(sz_vis, input.data);
    i++;
  }

  m_materialData.reset(new char[sz_vis.sz]);
  std::fill_n(m_materialData.get(), sz_vis.sz, 0);
  char* cur = m_materialData.get();

  input_port_vis visitor{*this, cur};
  for (const isf::input& input : desc.inputs)
    std::visit(visitor, input.data);

  output.push_back(new Port{this, {}, Types::Image, {}});
}

const Mesh& ISFNode::mesh() const noexcept
{
  return *this->m_mesh;
}

struct RenderedISFNode : RenderedNode
{
  using RenderedNode::RenderedNode;

  void customInit(Renderer& renderer) override
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& n = (ISFNode&)(node);
    for (auto& texture : n.audio_textures)
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();

      m_samplers.push_back({sampler, renderer.m_emptyTexture});
      texture.samplers[&renderer] = {sampler, nullptr};
    }
  }

  void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& n = (ISFNode&)node;
    for (auto& audio : n.audio_textures)
    {
      bool textureChanged = false;
      auto& [rhiSampler, rhiTexture] = audio.samplers[&renderer];
      const auto curSz = (rhiTexture) ? rhiTexture->pixelSize() : QSize{};
      int numSamples = curSz.width() * curSz.height();
      if (numSamples != audio.data.size())
      {
        delete rhiTexture;
        rhiTexture = nullptr;
        textureChanged = true;
      }

      if (!rhiTexture)
      {
        if (audio.channels > 0)
        {
          int samples = audio.data.size() / audio.channels;
          rhiTexture = rhi.newTexture(
              QRhiTexture::D32F, {samples, audio.channels}, 1, QRhiTexture::Flag{});
          rhiTexture->build();
          textureChanged = true;
        }
        else
        {
          rhiTexture = nullptr;
          textureChanged = true;
        }
      }

      if (textureChanged)
      {
        replaceTexture(rhiSampler, rhiTexture ? rhiTexture : renderer.m_emptyTexture);
      }

      if (rhiTexture)
      {
        QRhiTextureSubresourceUploadDescription subdesc(audio.data.data(), audio.data.size() * 4);
        QRhiTextureUploadEntry entry{0, 0, subdesc};
        QRhiTextureUploadDescription desc{entry};
        res.uploadTexture(rhiTexture, desc);
      }
    }
  }

  void customRelease(Renderer& renderer) override
  {
    auto& n = (ISFNode&)(node);
    for (auto& texture : n.audio_textures)
      if (auto tex = texture.samplers[&renderer].second)
      {
        if (tex != renderer.m_emptyTexture)
          tex->releaseAndDestroyLater();
      }
  }
};

RenderedNode* ISFNode::createRenderer() const noexcept
{
  return new RenderedISFNode{*this};
}
