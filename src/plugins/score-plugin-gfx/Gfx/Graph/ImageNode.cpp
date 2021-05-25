#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/detail/math.hpp>

namespace score::gfx
{

static const constexpr auto images_vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding = 3) uniform sampler2D y_tex;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
};

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";

static const constexpr auto images_fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
};

layout(std140, binding = 2) uniform material_t {
  int idx;
  float opacity;
  vec2 position;
  vec2 scale;
};

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 factor = textureSize(y_tex, 0) / renderSize;
  vec2 ifactor = renderSize / textureSize(y_tex, 0);
  vec2 texcoord = vec2(v_texcoord.x, texcoordAdjust.y + texcoordAdjust.x * v_texcoord.y);
  texcoord = vec2(1) - ifactor * position + texcoord / factor;
  texcoord = texcoord / scale;
  float actual = texcoord.x >= 0 && texcoord.x <= 1 && texcoord.y >= 0 && texcoord.y <= 1 ? 1.0f : 0.0f;
  fragColor = texture(y_tex, texcoord) * opacity * actual;
}
)_";
ImagesNode::ImagesNode(std::vector<score::gfx::Image> dec)
    : images{std::move(dec)}
{
  std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(images_vertex_shader, images_fragment_shader);
  input.push_back(new Port{this, &ubo.currentImageIndex, Types::Int, {}});
  input.push_back(new Port{this, &ubo.opacity, Types::Float, {}});
  input.push_back(new Port{this, &ubo.position[0], Types::Vec2, {}});
  input.push_back(new Port{this, &ubo.scale[0], Types::Vec2, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});

  m_materialData.reset((char*)&ubo);
}

ImagesNode::~ImagesNode() { m_materialData.release(); }

const Mesh& ImagesNode::mesh() const noexcept  { return this->m_mesh; }

#include <Gfx/Qt5CompatPush> // clang-format: keep
class ImagesNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~Renderer() { }

  void customInit(RenderList& renderer) override
  {
    defaultShaderMaterialInit(renderer);

    prev_ubo.currentImageIndex = -1;
    auto& n = static_cast<const ImagesNode&>(this->node);
    auto& rhi = *renderer.state.rhi;
    for (const score::gfx::Image& img : n.images)
    {
      for (const QImage& frame : img.frames)
      {
        const QSize sz = frame.size();
        auto tex = rhi.newTexture(
            QRhiTexture::BGRA8,
            QSize{sz.width(), sz.height()},
            1,
            QRhiTexture::Flag{});

        tex->create();
        textures.push_back(tex);
      }
    }

    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);

      sampler->create();
      auto tex
          = textures.empty() ? renderer.m_emptyTexture : textures.front();
      m_samplers.push_back({sampler, tex});
    }
  }

  void
  customUpdate(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if (textures.empty())
      return;

    auto& n = static_cast<const ImagesNode&>(this->node);
    if (!m_uploaded)
    {
      int k = 0;
      for (std::size_t i = 0, N = n.images.size(); i < N; i++)
      {
        for (const auto& frame : n.images[i].frames)
        {
          res.uploadTexture(textures[k], frame);
          k++;
        }
      }
      m_uploaded = true;
    }

    if (prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
    {
      if (!textures.empty())
      {
        auto idx = ossia::clamp(
            int(n.ubo.currentImageIndex), int(0), int(textures.size()) - 1);

        score::gfx::replaceTexture(
            *m_p.srb, m_samplers[0].sampler, textures[idx]);
      }
      prev_ubo.currentImageIndex = n.ubo.currentImageIndex;
    }
  }

  void customRelease(RenderList&) override
  {
    for (auto tex : textures)
    {
      tex->deleteLater();
    }
    textures.clear();
  }

  struct ImagesNode::UBO prev_ubo;
  std::vector<QRhiTexture*> textures;
  bool m_uploaded = false;
  /*
    std::optional<QSize> renderTargetSize() const noexcept override
    {
      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      return QSize{w, h};
    }*/
};
#include <Gfx/Qt5CompatPop> // clang-format: keep

NodeRenderer* ImagesNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

}
