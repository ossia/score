#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

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
  texcoord = vec2(1) - ifactor * vec2(position.x, 1. - position.y) + texcoord / factor;
  texcoord = texcoord / scale;
  float actual = texcoord.x >= 0 && texcoord.x <= 1 && texcoord.y >= 0 && texcoord.y <= 1 ? 1.0f : 0.0f;
  fragColor = texture(y_tex, texcoord) * opacity * actual;
}
)_";
ImagesNode::ImagesNode(std::vector<score::gfx::Image> dec)
    : images{std::move(dec)}
{
  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(images_vertex_shader, images_fragment_shader);
  input.push_back(new Port{this, &ubo.currentImageIndex, Types::Int, {}});
  input.push_back(new Port{this, &ubo.opacity, Types::Float, {}});
  input.push_back(new Port{this, &ubo.position[0], Types::Vec2, {}});
  input.push_back(new Port{this, &ubo.scale[0], Types::Vec2, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});

  m_materialData.reset((char*)&ubo);
}

ImagesNode::~ImagesNode()
{
  m_materialData.release();
}

const Mesh& ImagesNode::mesh() const noexcept
{
  return this->m_mesh;
}

#include <Gfx/Qt5CompatPush> // clang-format: keep
class ImagesNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~Renderer() { }

  TextureRenderTarget renderTargetForInput(const Port& p) override { return { }; }
  void init(RenderList& renderer) override
  {
    defaultMeshInit(renderer);
    defaultUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_prev_ubo.currentImageIndex = -1;
    auto& n = static_cast<const ImagesNode&>(this->node);
    QRhi& rhi = *renderer.state.rhi;
    const int limits_min = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
    const int limits_max = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

    // Create GPU textures for each image
    for (const score::gfx::Image& img : n.images)
    {
      for (const QImage& frame : img.frames)
      {
        const QSize sz = frame.size();
        auto tex = rhi.newTexture(
            QRhiTexture::BGRA8,
            resizeTextureSize(QSize{sz.width(), sz.height()}, limits_min, limits_max),
            1,
            QRhiTexture::Flag{});

        tex->setName("ImagesNode::tex");
        tex->create();
        m_textures.push_back(tex);
      }
    }

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::Mirror,
          QRhiSampler::Mirror);

      sampler->setName("ImagesNode::sampler");
      sampler->create();
      auto tex = m_textures.empty() ? &renderer.emptyTexture() : m_textures.front();
      m_samplers.push_back({sampler, tex});
    }

    defaultPassesInit(renderer);
  }

  void
  update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    defaultUBOUpdate(renderer, res);

    if (m_textures.empty())
      return;

    auto& n = static_cast<const ImagesNode&>(this->node);
    // If images haven't been uploaded yet, upload them.
    if (!m_uploaded)
    {
      const int limits_min = renderer.state.rhi->resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
      const int limits_max = renderer.state.rhi->resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

      int k = 0;
      for (std::size_t i = 0, N = n.images.size(); i < N; i++)
      {
        for (const auto& frame : n.images[i].frames)
        {
          res.uploadTexture(m_textures[k], resizeTexture(frame, limits_min, limits_max));
          k++;
        }
      }
      m_uploaded = true;
    }

    // If the current image being displayed by this renderer (in m_prev_ubo)
    // is out of date with the image in the data model, we switch the texture
    if (m_prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
    {
      if (!m_textures.empty())
      {
        auto idx = ossia::clamp(
            int(n.ubo.currentImageIndex), int(0), int(m_textures.size()) - 1);

        for(auto& pass : m_p)
        {
          score::gfx::replaceTexture(
              *pass.second.srb, m_samplers[0].sampler, m_textures[idx]);
        }
      }
      m_prev_ubo.currentImageIndex = n.ubo.currentImageIndex;
    }
  }

  void release(RenderList& r) override
  {
    for (auto tex : m_textures)
    {
      tex->deleteLater();
    }
    m_textures.clear();

    defaultRelease(r);
  }

  struct ImagesNode::UBO m_prev_ubo;
  std::vector<QRhiTexture*> m_textures;
  bool m_uploaded = false;
};
#include <Gfx/Qt5CompatPop> // clang-format: keep

NodeRenderer* ImagesNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

}


namespace score::gfx
{

static const constexpr auto fullscreen_images_vertex_shader = R"_(#version 450
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

static const constexpr auto fullscreen_images_fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
};

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 factor = textureSize(y_tex, 0) / renderSize;
  vec2 ifactor = renderSize / textureSize(y_tex, 0);
  vec2 texcoord = vec2(v_texcoord.x, texcoordAdjust.y + texcoordAdjust.x * v_texcoord.y);
/*
  texcoord = vec2(1) - ifactor * position + texcoord / factor;
  texcoord = texcoord / scale;
*/
  fragColor = texture(y_tex, texcoord);
}
)_";
FullScreenImageNode::FullScreenImageNode(QImage dec)
    : m_image{std::move(dec)}
{
  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(fullscreen_images_vertex_shader, fullscreen_images_fragment_shader);
  output.push_back(new Port{this, {}, Types::Image, {}});
}

FullScreenImageNode::~FullScreenImageNode()
{
}

const Mesh& FullScreenImageNode::mesh() const noexcept
{
  return this->m_mesh;
}

#include <Gfx/Qt5CompatPush> // clang-format: keep
class FullScreenImageNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~Renderer() { }

  TextureRenderTarget renderTargetForInput(const Port& p) override { return { }; }
  void init(RenderList& renderer) override
  {
    defaultMeshInit(renderer);
    defaultUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    auto& n = static_cast<const FullScreenImageNode&>(this->node);
    auto& rhi = *renderer.state.rhi;

    // Create GPU textures for the image
    const QSize sz = n.m_image.size();
    auto tex = rhi.newTexture(
        QRhiTexture::BGRA8,
        QSize{sz.width(), sz.height()},
        1,
        QRhiTexture::Flag{});

    tex->setName("FullScreenImageNode::tex");
    tex->create();
    m_texture = tex;

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);

      sampler->setName("FullScreenImageNode::sampler");
      sampler->create();
      m_samplers.push_back({sampler, m_texture});
    }

    defaultPassesInit(renderer);
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    defaultUBOUpdate(renderer, res);

    auto& n = static_cast<const FullScreenImageNode&>(this->node);
    // If images haven't been uploaded yet, upload them.
    if (!m_uploaded)
    {
      res.uploadTexture(m_texture, n.m_image);
      m_uploaded = true;
    }
  }

  void release(RenderList& r) override
  {
    m_texture->deleteLater();
    m_texture = nullptr;

    defaultRelease(r);
  }

  QRhiTexture* m_texture{};
  bool m_uploaded = false;
};
#include <Gfx/Qt5CompatPop> // clang-format: keep

NodeRenderer* FullScreenImageNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

}
