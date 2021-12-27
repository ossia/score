#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <Gfx/Images/Process.hpp>


#include <ossia/gfx/port_index.hpp>
#include <ossia/detail/math.hpp>

namespace score::gfx
{
static int imageIndex(int idx, int size)
{
  if(size > 0)
  {
    idx %= size;
    if(idx < 0)
      idx += size;

    SCORE_ASSERT(idx >= 0);
    SCORE_ASSERT(idx < size);

    return idx;
  }
  else
  {
    return 0;
  }
}
static const constexpr auto images_single_vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding = 3) uniform sampler2D y_tex;
layout(location = 0) out vec2 v_texcoord;

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
  vec2 imageSize;
} mat;
out gl_PerVertex { vec4 gl_Position; };

void main()
{
  float viewportAspect = renderSize.x / renderSize.y;
  float imageAspect = mat.imageSize.x / mat.imageSize.y;

  vec2 pos = position;
  // Aspect ratio
  // Our mesh is: -1, -1;  1, -1;  -1, 1;  1, 1;

  pos.x /= viewportAspect;
  pos.y /= imageAspect;

  // User scale
  pos *= mat.scale;

  // User displacement
  pos += mat.position;

  v_texcoord = vec2(texcoord.x, texcoordAdjust.y + texcoordAdjust.x * texcoord.y);
  gl_Position = clipSpaceCorrMatrix * vec4(pos, 0.0, 1.);
}
)_";

static const constexpr auto images_single_fragment_shader = R"_(#version 450
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
  vec2 imageSize;
} mat;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec4 tex = texture(y_tex, v_texcoord);
  fragColor = tex * mat.opacity;
}
)_";

static const constexpr auto images_tiled_fragment_shader = R"_(#version 450
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
  vec2 imageSize;
} mat;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  float viewportAspect = renderSize.x / renderSize.y;
  float imageAspect = mat.imageSize.x / mat.imageSize.y;

  vec2 pos = v_texcoord;
  // Aspect ratio
  // Our mesh is: -1, -1;  1, -1;  -1, 1;  1, 1;

  pos.x /= viewportAspect;
  pos.y /= imageAspect;

  vec4 tex = texture(y_tex, pos * mat.scale + mat.position);
  fragColor = tex * mat.opacity;
}
)_";
ImagesNode::ImagesNode()
{
  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(images_single_vertex_shader, images_single_fragment_shader);
  input.push_back(new Port{this, &ubo.currentImageIndex, Types::Int, {}});
  input.push_back(new Port{this, &ubo.opacity, Types::Float, {}});
  input.push_back(new Port{this, &ubo.position[0], Types::Vec2, {}});
  input.push_back(new Port{this, &ubo.scale[0], Types::Vec2, {}});
  input.push_back(new Port{this, &ubo.imageSize[0], Types::Vec2, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});

  m_materialData.reset((char*)&ubo);
}

void ImagesNode::process(const Message& msg)
{
  ProcessNode::process(msg.token);

  int32_t p = 0;
  for (const gfx_input& m: msg.input)
  {
    if(auto val = std::get_if<ossia::value>(&m))
    {
      switch(p)
      {
        case 0: // Image index
        {
          auto sink = ossia::gfx::port_index{msg.node_id, p };
          std::visit([this, sink] (const auto& v) { ProcessNode::process(sink.port, v); }, std::move(m));

          if(linearImages.size() > 0)
          {
            const int idx = imageIndex(ubo.currentImageIndex, linearImages.size());
            auto sz = linearImages[idx]->size();
            ubo.imageSize[0] = sz.width();
            ubo.imageSize[1] = sz.height();
          }

          break;
        }
        case 1: // Opacity
        {
          auto opacity = ossia::convert<float>(*val);
          this->ubo.opacity = ossia::clamp(opacity, 0.f, 1.f);
          this->materialChanged++;
          break;
        }
        case 2: // Position
        {
          auto sink = ossia::gfx::port_index{msg.node_id, p };
          std::visit([this, sink] (const auto& v) { ProcessNode::process(sink.port, v); }, std::move(m));
          break;
        }

        case 3: // X scale
        {
          {
            auto scale = ossia::convert<float>(*val);
            this->ubo.scale[0] = scale;
            this->materialChanged++;
          }
          break;
        }
        case 4: // Scale Y
        {
          {
            auto scale = ossia::convert<float>(*val);
            this->ubo.scale[1] = scale;
            this->materialChanged++;
          }
          break;
        }

        case 5: // Images
        {
          {
            linearImages.clear();
            Gfx::releaseImages(images);
            images = Gfx::getImages(*val);
            for(auto& img : images)
            {
              for(auto& frame : img.frames)
              {
                linearImages.push_back(&frame);
              }
            }

            if(linearImages.size() > 0)
            {
              const int idx = imageIndex(ubo.currentImageIndex, linearImages.size());
              auto sz = linearImages[idx]->size();
              ubo.imageSize[0] = sz.width();
              ubo.imageSize[1] = sz.height();
            }

            ++this->imagesChanged;
          }
          break;
        }

        case 6: // Tile
        {
          {
            this->tile = ossia::convert<bool>(*val);
            ++this->materialChanged;
          }
          break;
        }
      }
    }

    p++;
  }

}

ImagesNode::~ImagesNode()
{
  Gfx::releaseImages(images);
  m_materialData.release();
}

#include <Gfx/Qt5CompatPush> // clang-format: keep
class ImagesNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~Renderer() { }

  int imagesChanged = -1;
  bool tile{};

  void recreateTextures(QRhi& rhi)
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    const int limits_min = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
    const int limits_max = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

    for (const QImage* frame : n.linearImages)
    {
      const QSize sz = frame->size();
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

  TextureRenderTarget renderTargetForInput(const Port& p) override { return { }; }
  void init(RenderList& renderer) override
  {
    const TexturedQuad& mesh = TexturedQuad::instance();
    defaultMeshInit(renderer, mesh);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_prev_ubo.currentImageIndex = -1;
    QRhi& rhi = *renderer.state.rhi;

    // Create GPU textures for each image
    recreateTextures(rhi);

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

    defaultPassesInit(renderer, mesh);
  }

  void
  update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    if(n.tile != tile)
    {
      release(renderer);
      tile = n.tile;
      auto& nn = const_cast<ImagesNode&>(n);
      QShader& v = nn.m_vertexS, &f = nn.m_fragmentS;;
      if(!tile)
        std::tie(v, f) = score::gfx::makeShaders(images_single_vertex_shader, images_single_fragment_shader);
      else
        std::tie(v, f) = score::gfx::makeShaders(TexturedTriangle{}.defaultVertexShader(), images_tiled_fragment_shader);
      init(renderer);
    }
    bool updateCurrentTexture = false;
    if(n.imagesChanged > imagesChanged)
    {
      imagesChanged = n.imagesChanged;
      for(auto tex : m_textures)
      {
        tex->deleteLater();
      }
      m_textures.clear();

      recreateTextures(*renderer.state.rhi);
      m_uploaded = false;
    }

    // If images haven't been uploaded yet, upload them.
    if (!m_uploaded)
    {
      const int limits_min = renderer.state.rhi->resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
      const int limits_max = renderer.state.rhi->resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

      std::size_t k = 0;
      for (const QImage* frame : n.linearImages)
      {
        res.uploadTexture(m_textures[k], resizeTexture(*frame, limits_min, limits_max));
        k++;
      }
      m_uploaded = true;
      updateCurrentTexture = true;
    }


    // If the current image being displayed by this renderer (in m_prev_ubo)
    // is out of date with the image in the data model, we switch the texture
    if (updateCurrentTexture || m_prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
    {
      auto& sampler = m_samplers[0].sampler;
      if (!m_textures.empty())
      {
        const int idx = imageIndex(n.ubo.currentImageIndex, m_textures.size());

        for(auto& pass : m_p)
        {
          score::gfx::replaceTexture(
              *pass.second.srb, sampler, m_textures[idx]);
        }
      }
      else
      {
        for(auto& pass : m_p)
        {
          score::gfx::replaceTexture(
              *pass.second.srb, sampler, &renderer.emptyTexture());
        }
      }
      m_prev_ubo.currentImageIndex = n.ubo.currentImageIndex;
    }

    defaultUBOUpdate(renderer, res);
  }

  void runRenderPass(
      RenderList& renderer,
      QRhiCommandBuffer& cb,
      Edge& edge) override
  {
    auto& mesh = TexturedQuad::instance();
    defaultRenderPass(renderer, mesh, cb, edge);
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
    const TexturedTriangle& mesh = TexturedTriangle::instance();
    defaultMeshInit(renderer, mesh);
    processUBOInit(renderer);
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

    defaultPassesInit(renderer, mesh);
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

  void runRenderPass(
      RenderList& renderer,
      QRhiCommandBuffer& cb,
      Edge& edge) override
  {
    auto& mesh = TexturedTriangle::instance();
    defaultRenderPass(renderer, mesh, cb, edge);
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
