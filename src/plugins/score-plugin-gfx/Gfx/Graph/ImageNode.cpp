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

  v_texcoord = texcoord;
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

vec2 norm_texcoord(vec2 tc)
{
  vec2 tex_sz = textureSize(y_tex, 0);
  return tc * mat.imageSize / tex_sz;
}

void main ()
{
  fragColor = texture(y_tex, norm_texcoord(v_texcoord)) * mat.opacity;
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

vec2 norm_texcoord(vec2 tc)
{
  vec2 tex_sz = textureSize(y_tex, 0);
  return tc * mat.imageSize / tex_sz;
}

void main ()
{
  float viewportAspect = renderSize.x / renderSize.y;
  float imageAspect = mat.imageSize.x / mat.imageSize.y;

  vec2 pos = v_texcoord;
  // Aspect ratio
  // Our mesh is: -1, -1;  1, -1;  -1, 1;  1, 1;

  pos.x /= viewportAspect;
  pos.y /= imageAspect;

  vec4 tex = texture(y_tex, norm_texcoord(pos / mat.scale + mat.position));
  fragColor = tex * mat.opacity;
}
)_";
ImagesNode::ImagesNode()
{
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
    if(auto val = ossia::get_if<ossia::value>(&m))
    {
      switch(p)
      {
        case 0: // Image index
        {
          auto sink = ossia::gfx::port_index{msg.node_id, p };
          ossia::visit([this, sink] (const auto& v) { ProcessNode::process(sink.port, v); }, std::move(m));

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
          ossia::visit([this, sink] (const auto& v) { ProcessNode::process(sink.port, v); }, std::move(m));
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
            this->tile = (ImageMode)ossia::convert<int>(*val);
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
static QRhiSampler* createSampler(ImageMode tile, QRhi& rhi)
{
  QRhiSampler::AddressMode am{};
  switch(tile) {
    default:
    case Single: am = QRhiSampler::ClampToEdge; break;
    case Clamped: am = QRhiSampler::ClampToEdge; break;
    case Tiled: am = QRhiSampler::Repeat; break;
    case Mirrored: am = QRhiSampler::Mirror; break;
  }

  auto sampler = rhi.newSampler(
      QRhiSampler::Linear,
      QRhiSampler::Linear,
      QRhiSampler::None,
      am,
      am);

  sampler->setName("ImagesNode::sampler");
  sampler->create();
  return sampler;
}
class ImagesNode::PreloadedRenderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~PreloadedRenderer() { }

  int imagesChanged = -1;
  ImageMode tile{};

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
    auto& n = static_cast<const ImagesNode&>(this->node);
    const auto& rs = renderer.state;
    const Mesh& mesh = renderer.defaultQuad();
    defaultMeshInit(renderer, mesh);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_prev_ubo.currentImageIndex = -1;
    QRhi& rhi = *renderer.state.rhi;

    // Create GPU textures for each image
    recreateTextures(rhi);

    tile = n.tile;
    QShader& v = m_vertexS, &f = m_fragmentS;
    if(!tile)
      std::tie(v, f) = score::gfx::makeShaders(rs, images_single_vertex_shader, images_single_fragment_shader);
    else
      std::tie(v, f) = score::gfx::makeShaders(rs, TexturedTriangle{}.defaultVertexShader(), images_tiled_fragment_shader);

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = createSampler(tile, rhi);
      auto tex = m_textures.empty() ? &renderer.emptyTexture() : m_textures.front();
      m_samplers.push_back({sampler, tex});
    }

    // Initialize the passes for the "single" case
    defaultPassesInit(renderer, mesh);

    // Initialize the passes for the "tiled" case
    {
      auto [v, f] = score::gfx::makeShaders(rs, TexturedTriangle{}.defaultVertexShader(), images_tiled_fragment_shader);
      for(Edge* edge : this->node.output[0]->edges)
      {
        auto rt = renderer.renderTargetForOutput(*edge);
        if(rt.renderTarget)
        {
          m_altPasses.emplace_back(edge, score::gfx::buildPipeline(
                                     renderer,
                                     mesh,
                                     v,
                                     f,
                                     rt,
                                     m_processUBO,
                                     m_material.buffer, m_samplers));
        }
      }
    }
  }

  void
  update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    if(n.tile != tile)
    {
      tile = n.tile;
      auto [s, tex] = m_samplers[0];
          m_samplers.clear();

          // Create a new sampler
          auto new_sampler = createSampler(tile, *renderer.state.rhi);
          m_samplers.push_back({new_sampler, tex});

      // Replace it in the render passes
      auto replace_sampler = [] (PassMap& passes, QRhiSampler* oldS, QRhiSampler* newS) {
        for(auto& pass : passes)
          score::gfx::replaceSampler(*pass.second.srb, oldS, newS);
      };

      replace_sampler(m_p, s, new_sampler);
      replace_sampler(m_altPasses, s, new_sampler);

      // Release the old sampler
      s->deleteLater();
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
      std::size_t k = 0;
      for (const QImage* frame : n.linearImages)
      {
        res.uploadTexture(m_textures[k], renderer.adaptImage(*frame));
        k++;
      }
      m_uploaded = true;
      updateCurrentTexture = true;
    }

    // If the current image being displayed by this renderer (in m_prev_ubo)
    // is out of date with the image in the data model, we switch the texture
    if (updateCurrentTexture || m_prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
    {
      auto replace_texture = [] (PassMap& passes, QRhiSampler* sampler, QRhiTexture* tex) {
        for(auto& pass : passes)
          score::gfx::replaceTexture(*pass.second.srb, sampler, tex);
      };

      const int idx = imageIndex(n.ubo.currentImageIndex, m_textures.size());
      QRhiSampler* sampler = m_samplers[0].sampler;
      QRhiTexture* new_tex{};
      if(ossia::valid_index(idx, m_textures))
      {
        new_tex = m_textures[idx];
      }
      else
      {
        new_tex = &renderer.emptyTexture();
      }

      replace_texture(m_p, sampler, new_tex);
      replace_texture(m_altPasses, sampler, new_tex);

      m_prev_ubo.currentImageIndex = n.ubo.currentImageIndex;
    }

    defaultUBOUpdate(renderer, res);
  }

  void runRenderPass(
      RenderList& renderer,
      QRhiCommandBuffer& cb,
      Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    if(tile == ImageMode::Single)
      defaultRenderPass(renderer, mesh, cb, edge, m_p);
    else
      defaultRenderPass(renderer, mesh, cb, edge, m_altPasses);
  }

  void release(RenderList& r) override
  {
    for (auto tex : m_textures)
    {
      tex->deleteLater();
    }
    m_textures.clear();

    defaultRelease(r);

    {
      for(auto& pass : m_altPasses)
        pass.second.release();
      m_altPasses.clear();
    }
  }

  struct ImagesNode::UBO m_prev_ubo;
  ossia::small_vector<std::pair<Edge*, Pipeline>, 2> m_altPasses;
  std::vector<QRhiTexture*> m_textures;
  bool m_uploaded = false;
};

class ImagesNode::OnTheFlyRenderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~OnTheFlyRenderer() { }

  int imagesChanged = -1;
  ImageMode tile{};

  void recreateTexture(QRhi& rhi)
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    const int limits_min = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
    const int limits_max = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

    QSize maxSize{1, 1};
    for (const QImage* frame : n.linearImages)
    {
      const auto sz = resizeTextureSize(frame->size(), limits_min, limits_max);

      maxSize.setWidth(std::max(maxSize.width(), sz.width()));
      maxSize.setHeight(std::max(maxSize.height(), sz.height()));
    }

    auto tex = rhi.newTexture(QRhiTexture::BGRA8, maxSize, 1, QRhiTexture::Flag{});

    tex->setName("OnTheFlyRenderer::tex");
    tex->create();

    m_texture = tex;
  }

  TextureRenderTarget renderTargetForInput(const Port& p) override { return { }; }
  void init(RenderList& renderer) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);
    const auto& rs = renderer.state;
    const auto& mesh = renderer.defaultQuad();
    defaultMeshInit(renderer, mesh);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_prev_ubo.currentImageIndex = -1;
    QRhi& rhi = *renderer.state.rhi;


    tile = n.tile;
    QShader& v = m_vertexS, &f = m_fragmentS;
    if(!tile)
      std::tie(v, f) = score::gfx::makeShaders(rs, images_single_vertex_shader, images_single_fragment_shader);
    else
      std::tie(v, f) = score::gfx::makeShaders(rs, TexturedTriangle{}.defaultVertexShader(), images_tiled_fragment_shader);

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = createSampler(tile, rhi);

      // Create GPU texture
      recreateTexture(rhi);

      m_samplers.push_back({sampler, m_texture});
    }

    // Initialize the passes for the "single" case
    defaultPassesInit(renderer, mesh);

    // Initialize the passes for the "tiled" case
    {
      auto [v, f] = score::gfx::makeShaders(rs, TexturedTriangle{}.defaultVertexShader(), images_tiled_fragment_shader);
      for(Edge* edge : this->node.output[0]->edges)
      {
        auto rt = renderer.renderTargetForOutput(*edge);
        if(rt.renderTarget)
        {
          m_altPasses.emplace_back(edge, score::gfx::buildPipeline(
                             renderer,
                             mesh,
                             v,
                             f,
                             rt,
                             m_processUBO,
                             m_material.buffer, m_samplers));
        }
      }
    }
  }


  void
  update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    if(n.tile != tile)
    {
      tile = n.tile;
      auto [s, tex] = m_samplers[0];

      m_samplers.clear();

      // Create a new sampler
      auto new_sampler = createSampler(tile, *renderer.state.rhi);
      m_samplers.push_back({new_sampler, tex});

      // Replace it in the render passes
      auto replace_sampler = [] (PassMap& passes, QRhiSampler* oldS, QRhiSampler* newS) {
        for(auto& pass : passes)
          score::gfx::replaceSampler(*pass.second.srb, oldS, newS);
      };

      replace_sampler(m_p, s, new_sampler);
      replace_sampler(m_altPasses, s, new_sampler);

      // Release the old sampler
      s->deleteLater();
    }

    if(n.imagesChanged > imagesChanged)
    {
      imagesChanged = n.imagesChanged;
      m_texture->deleteLater();
      m_texture = nullptr;

      recreateTexture(*renderer.state.rhi);
      m_uploaded = false;

      auto replace_texture = [] (PassMap& passes, QRhiSampler* sampler, QRhiTexture* tex) {
        for(auto& pass : passes)
          score::gfx::replaceTexture(*pass.second.srb, sampler, tex);
      };

      auto sampler = m_samplers[0].sampler;
      replace_texture(m_p, sampler, m_texture);
      replace_texture(m_altPasses, sampler, m_texture);
    }

    // If the current image being displayed by this renderer (in m_prev_ubo)
    // is out of date with the image in the data model, we switch the texture
    if (!m_uploaded || m_prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
    {
      const int idx = imageIndex(n.ubo.currentImageIndex, n.linearImages.size());
      if(ossia::valid_index(idx, n.linearImages))
      {
        auto frame = n.linearImages[idx];

        res.uploadTexture(m_texture, renderer.adaptImage(*frame));

        m_prev_ubo.currentImageIndex = n.ubo.currentImageIndex;
        m_uploaded = true;
      }
    }

    defaultUBOUpdate(renderer, res);
  }

  void runRenderPass(
      RenderList& renderer,
      QRhiCommandBuffer& cb,
      Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    if(tile == ImageMode::Single)
      defaultRenderPass(renderer, mesh, cb, edge, m_p);
    else
      defaultRenderPass(renderer, mesh, cb, edge, m_altPasses);
  }

  void release(RenderList& r) override
  {
    m_texture->deleteLater();
    m_texture = nullptr;

    defaultRelease(r);

    {
      for(auto& pass : m_altPasses)
        pass.second.release();
      m_altPasses.clear();
    }
  }

  struct ImagesNode::UBO m_prev_ubo;
  ossia::small_vector<std::pair<Edge*, Pipeline>, 2> m_altPasses;
  QRhiTexture* m_texture{};
  bool m_uploaded = false;
};
#include <Gfx/Qt5CompatPop> // clang-format: keep

NodeRenderer* ImagesNode::createRenderer(RenderList& r) const noexcept
{
  return new PreloadedRenderer{*this};
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
  fragColor = texture(y_tex, v_texcoord);
}
)_";
FullScreenImageNode::FullScreenImageNode(QImage dec)
    : m_image{std::move(dec)}
{
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
    const auto& mesh = renderer.defaultTriangle();
    defaultMeshInit(renderer, mesh);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, fullscreen_images_vertex_shader, fullscreen_images_fragment_shader);

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
      res.uploadTexture(m_texture, renderer.adaptImage(n.m_image));
      m_uploaded = true;
    }
  }

  void runRenderPass(
      RenderList& renderer,
      QRhiCommandBuffer& cb,
      Edge& edge) override
  {
    const auto& mesh = renderer.defaultTriangle();
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
