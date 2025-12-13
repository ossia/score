#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Images/Process.hpp>

#if QT_SVG_LIB
#include <QPainter>
#include <QSvgRenderer>
#endif

#include <ossia/detail/math.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value_conversion.hpp>

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
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  int idx;
  float opacity;
  vec2 position;
  vec2 scale;
} mat;
out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position * mat.scale + mat.position, 0.0, 1.);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto images_single_fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  int idx;
  float opacity;
  vec2 position;
  vec2 scale;
} mat;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, v_texcoord) * mat.opacity;
}
)_";

static const constexpr auto images_tiled_vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  int idx;
  float opacity;
  vec2 position;
  vec2 scale;
} mat;


out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto images_tiled_fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  int idx;
  float opacity;
  vec2 position;
  vec2 scale;
} mat;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, v_texcoord * mat.scale + (1.0f - mat.scale) / 2.0f + vec2(-mat.position.x, mat.position.y)) * mat.opacity;
}
)_";
ImagesNode::ImagesNode(const score::DocumentContext& ctx)
    : ctx{ctx}
{
  input.push_back(new Port{this, &ubo.currentImageIndex, Types::Int, {}});
  input.push_back(new Port{this, &ubo.opacity, Types::Float, {}});
  input.push_back(new Port{this, &ubo.position[0], Types::Vec2, {}});
  input.push_back(new Port{this, &ubo.scale[0], Types::Vec2, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});

  m_materialData.reset((char*)&ubo);
}

void ImagesNode::process(Message&& msg)
{
  ProcessNode::process(msg.token);

  int32_t p = 0;
  for(auto&& m : std::move(msg.input))
  {
    if(auto val = ossia::get_if<ossia::value>(&m))
    {
      switch(p)
      {
        case 0: // Image index
        {
          auto sink = ossia::gfx::port_index{msg.node_id, p};
          ossia::visit(
              [this, sink](const auto& v) { ProcessNode::process(sink.port, v); },
              std::move(m));

          break;
        }
        case 1: // Opacity
        {
          auto opacity = ossia::convert<float>(*val);
          this->ubo.opacity = ossia::clamp(opacity, 0.f, 1.f);
          this->materialChange();
          break;
        }
        case 2: // Position
        {
          auto sink = ossia::gfx::port_index{msg.node_id, p};
          ossia::visit(
              [this, sink](const auto& v) { ProcessNode::process(sink.port, v); },
              std::move(m));
          break;
        }

        case 3: // X scale
        {
          auto scale = ossia::convert<float>(*val);
          this->scale_w = scale;
          this->materialChange();
          break;
        }
        case 4: // Scale Y
        {
          auto scale = ossia::convert<float>(*val);
          this->scale_h = scale;
          this->materialChange();
          break;
        }

        case 5: // Images
        {
          auto new_images = Gfx::getImages(*val, this->ctx);
          auto diff = [](const score::gfx::Image& lhs, const score::gfx::Image& rhs) {
            return lhs.path != rhs.path;
          };
          bool images_changed = new_images.size() != images.size();
          if(!images_changed)
          {
            for(int i = 0; i < images.size(); i++)
              images_changed |= diff(images[i], new_images[i]);
          }

          if(images_changed)
          {
            clear();
            images = std::move(new_images);
            for(auto& img : images)
            {
#if Q_SVG_LIB
              if(img.path.endsWith("svg"))
              {
                auto renderer = new QSvgRenderer{img.path};
                if(renderer->animated())
                  renderer->setAnimationEnabled(true);
                renderer->setFramesPerSecond(60);
                linearImages.push_back(renderer);
              }
              else
#endif
              {
                for(auto& frame : img.frames)
                {
                  linearImages.push_back(&frame);
                }
              }
            }

            ++this->imagesChanged;
          }
          break;
        }

        case 6: // Tile
        {
          this->tileMode = (ImageMode)ossia::convert<int>(*val);
          this->materialChange();
          break;
        }

        case 7: // Scale
        {
          this->scaleMode = (ScaleMode)ossia::convert<int>(*val);
          this->materialChange();
          break;
        }
      }
    }

    p++;
  }
}

void ImagesNode::clear()
{
#if Q_SVG_LIB
  for(auto image : linearImages)
  {
    if(auto svg = std::get_if<QSvgRenderer*>(&image))
    {
      delete *svg;
    }
  }
#endif

  linearImages.clear();
  Gfx::releaseImages(images);
}

ImagesNode::~ImagesNode()
{
  clear();
  m_materialData.release();
}

static QRhiSampler* createSampler(ImageMode tile, QRhi& rhi)
{
  QRhiSampler::AddressMode am{};
  switch(tile)
  {
    default:
    case Single:
      am = QRhiSampler::ClampToEdge;
      break;
    case Clamped:
      am = QRhiSampler::ClampToEdge;
      break;
    case Tiled:
      am = QRhiSampler::Repeat;
      break;
    case Mirrored:
      am = QRhiSampler::Mirror;
      break;
  }

  auto sampler = rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, am, am);

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
  ScaleMode scale{score::gfx::ScaleMode::Original};
  QSizeF lastRenderSize;
  bool mustRecomputeSize{true};
  float scale_w{1.f};
  float scale_h{1.f};

  void recreateTextures(QRhi& rhi)
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    const int limits_min = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
    const int limits_max = rhi.resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

    for(int i = 0, N = n.linearImages.size(); i < N; i++)
    {
      auto frame = n.linearImages[i];
      QSize sz{limits_min, limits_min};
      if(auto qimage = std::get_if<QImage*>(&frame))
      {
        sz = (*qimage)->size();
      }
#if Q_SVG_LIB
      else if(auto svg_p = std::get_if<QSvgRenderer*>(&frame))
      {
        auto* svg = *svg_p;
        auto svg_size = svg->defaultSize();
        svg_size = QSize(
            svg_size.width() * std::abs(scale_w), svg_size.height() * std::abs(scale_h));
        sz = svg_size;
      }
#endif
      const auto tex_size
          = resizeTextureSize(QSize{sz.width(), sz.height()}, limits_min, limits_max);

      if(m_textures.size() <= i)
      {
        QRhiTexture* tex = tex
            = rhi.newTexture(QRhiTexture::BGRA8, tex_size, 1, QRhiTexture::Flag{});
        tex->setName("ImagesNode::tex");
        tex->create();
        m_textures.push_back(tex);
      }
      else if(m_textures[i]->pixelSize() != tex_size)
      {
        auto tex = m_textures[i];
        tex->destroy();
        tex->setPixelSize(tex_size);
        tex->create();
      }
    }
  }

  TextureRenderTarget renderTargetForInput(const Port& p) override { return {}; }
  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);
    const auto& rs = renderer.state;
    const Mesh& mesh = renderer.defaultQuad();

    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_ubo.currentImageIndex = -1;
    QRhi& rhi = *renderer.state.rhi;

    // Create GPU textures for each image
    recreateTextures(rhi);

    tile = n.tileMode;
    std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(
        rs, images_single_vertex_shader, images_single_fragment_shader);

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
      auto [v, f] = score::gfx::makeShaders(
          rs, images_tiled_vertex_shader, images_tiled_fragment_shader);
      for(Edge* edge : this->node.output[0]->edges)
      {
        auto rt = renderer.renderTargetForOutput(*edge);
        if(rt.renderTarget)
        {
          m_altPasses.emplace_back(
              edge, score::gfx::buildPipeline(
                        renderer, mesh, v, f, rt, m_processUBO, m_material.buffer,
                        m_samplers));
        }
      }
    }
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    auto& n = (static_cast<const ImagesNode&>(this->node));
    if(n.tileMode != tile)
    {
      tile = n.tileMode;
      auto [s, tex] = m_samplers[0];
      m_samplers.clear();

      // Create a new sampler
      auto new_sampler = createSampler(tile, *renderer.state.rhi);
      m_samplers.push_back({new_sampler, tex});

      // Replace it in the render passes
      auto replace_sampler = [](PassMap& passes, QRhiSampler* oldS, QRhiSampler* newS) {
        for(auto& pass : passes)
          score::gfx::replaceSampler(*pass.second.srb, oldS, newS);
      };

      replace_sampler(m_p, s, new_sampler);
      replace_sampler(m_altPasses, s, new_sampler);

      // Release the old sampler
      mustRecomputeSize = true;
      imagesChanged = -1;
      s->deleteLater();
    }

    bool updateCurrentTexture = false;
    if(n.imagesChanged > imagesChanged)
    {
      imagesChanged = n.imagesChanged;
      if(m_textures.size() > n.linearImages.size())
      {
        for(int i = n.linearImages.size(); i < m_textures.size(); i++)
        {
          m_textures[i]->deleteLater();
        }
        m_textures.resize(n.linearImages.size());
      }

      m_uploaded = false;
    }

    recreateTextures(*renderer.state.rhi);

    // If images haven't been uploaded yet, upload them.
    {
      static thread_local QImage temp_svg;

      std::size_t k = 0;
      for(auto frame : n.linearImages)
      {
        if(auto qimage = std::get_if<QImage*>(&frame))
        {
          if(!m_uploaded)
          {
            res.uploadTexture(m_textures[k], renderer.adaptImage(**qimage));
            updateCurrentTexture = true;
          }
        }
#if Q_SVG_LIB
        else if(auto svg = std::get_if<QSvgRenderer*>(&frame))
        {
          auto svg_size = (*svg)->defaultSize();
          svg_size = QSize(
              svg_size.width() * std::abs(scale_w),
              svg_size.height() * std::abs(scale_h));
          if(!svg_size.isEmpty())
          {
            if((*svg)->animated())
            {
              if(temp_svg.size() != svg_size)
                temp_svg = QImage(svg_size, QImage::Format_ARGB32);
              QPainter temp_svg_painter{&temp_svg};
              temp_svg.fill(0);
              (*svg)->render(&temp_svg_painter);
              res.uploadTexture(m_textures[k], renderer.adaptImage(temp_svg));
              updateCurrentTexture = true;
            }
            else
            {
              if(!m_uploaded)
              {
                if(temp_svg.size() != svg_size)
                  temp_svg = QImage(svg_size, QImage::Format_ARGB32);
                QPainter temp_svg_painter{&temp_svg};
                temp_svg.fill(0);
                (*svg)->render(&temp_svg_painter);
                res.uploadTexture(m_textures[k], renderer.adaptImage(temp_svg));
                updateCurrentTexture = true;
              }
            }
          }
        }
#endif

        k++;
      }
      m_uploaded = true;
    }

    // If the current image being displayed by this renderer (in m_prev_ubo)
    // is out of date with the image in the data model, we switch the texture
    int currentImageIndex = -1;
    if(updateCurrentTexture || m_ubo.currentImageIndex != n.ubo.currentImageIndex)
    {
      auto replace_texture
          = [](PassMap& passes, QRhiSampler* sampler, QRhiTexture* tex) {
        for(auto& pass : passes)
          score::gfx::replaceTexture(*pass.second.srb, sampler, tex);
      };

      currentImageIndex = imageIndex(n.ubo.currentImageIndex, m_textures.size());
      QRhiSampler* sampler = m_samplers[0].sampler;
      QRhiTexture* new_tex{};
      if(ossia::valid_index(currentImageIndex, m_textures))
      {
        new_tex = m_textures[currentImageIndex];
      }
      else
      {
        new_tex = &renderer.emptyTexture();
      }

      replace_texture(m_p, sampler, new_tex);
      replace_texture(m_altPasses, sampler, new_tex);

      m_ubo.currentImageIndex = n.ubo.currentImageIndex;
      mustRecomputeSize = true;
    }

    // Copy the model UBO into the renderer
    m_ubo = n.ubo;

    if(edge)
    {
      const QSizeF renderSize = renderer.renderSize(edge);
      if(mustRecomputeSize || lastRenderSize != renderSize || scale != n.scaleMode
         || scale_w != n.scale_w || scale_h != n.scale_h || materialChanged)
      {
        scale = n.scaleMode;
        lastRenderSize = renderSize;
        scale_w = n.scale_w;
        scale_h = n.scale_h;

        QSizeF textureSize{1, 1};

        if(currentImageIndex == -1)
          currentImageIndex = imageIndex(m_ubo.currentImageIndex, m_textures.size());
        if(currentImageIndex < std::ssize(m_textures))
          textureSize = m_textures[currentImageIndex]->pixelSize();

        if(currentImageIndex < n.linearImages.size())
        {
#if Q_SVG_LIB
          const bool is_svg = n.linearImages[currentImageIndex].index() == 1;
          if(is_svg)
          {
            if(tile == score::gfx::Single)
            {
              auto sz = computeScaleForMeshSizing(scale, renderSize, textureSize);
              m_ubo.scale[0] = sz.width();
              m_ubo.scale[1] = sz.height();
            }
            else
            {
              auto sz = computeScaleForTexcoordSizing(scale, renderSize, textureSize);
              m_ubo.scale[0] = sz.width();
              m_ubo.scale[1] = sz.height();
            }
          }
          else
#endif
          {
            if(tile == score::gfx::Single)
            {
              auto sz = computeScaleForMeshSizing(scale, renderSize, textureSize);
              m_ubo.scale[0] = sz.width() * scale_w;
              m_ubo.scale[1] = sz.height() * scale_h;
            }
            else
            {
              auto sz = computeScaleForTexcoordSizing(scale, renderSize, textureSize);
              m_ubo.scale[0] = sz.width() / scale_w;
              m_ubo.scale[1] = sz.height() / scale_h;
            }
          }
        }

        materialChanged = true;
        mustRecomputeSize = false;
      }
    }

    // We can't use generic update since we need some modifications on the UBO
    // depending on the output resolution, so each renderer needs its own UBO:
    // GenericNodeRenderer::update(renderer, res, edge);

    defaultMeshUpdate(renderer, res);

    // FIXME check if it is actually used by the shaders
    res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &n.standardUBO);

    if(m_material.buffer && m_material.size > 0)
    {
      if(materialChanged)
      {
        res.updateDynamicBuffer(m_material.buffer, 0, m_material.size, &m_ubo);
      }
    }
  }

  void runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    if(tile == ImageMode::Single)
      defaultRenderPass(renderer, mesh, cb, edge, m_p);
    else
      defaultRenderPass(renderer, mesh, cb, edge, m_altPasses);
  }

  void release(RenderList& r) override
  {
    for(auto tex : m_textures)
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

  struct ImagesNode::UBO m_ubo;
  ossia::small_vector<std::pair<Edge*, Pipeline>, 2> m_altPasses;
  std::vector<QRhiTexture*> m_textures;
  bool m_uploaded = false;
};

#if 0
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
    for(const auto frame : n.linearImages)
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

  TextureRenderTarget renderTargetForInput(const Port& p) override { return {}; }
  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);
    const auto& rs = renderer.state;
    const auto& mesh = renderer.defaultQuad();
    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_prev_ubo.currentImageIndex = -1;
    QRhi& rhi = *renderer.state.rhi;

    tile = n.tileMode;
    QShader &v = m_vertexS, &f = m_fragmentS;
    if(!tile)
      std::tie(v, f) = score::gfx::makeShaders(
          rs, images_single_vertex_shader, images_single_fragment_shader);
    else
      std::tie(v, f) = score::gfx::makeShaders(
          rs, images_tiled_vertex_shader, images_tiled_fragment_shader);

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
      auto [v, f] = score::gfx::makeShaders(
          rs, images_tiled_vertex_shader, images_tiled_fragment_shader);
      for(Edge* edge : this->node.output[0]->edges)
      {
        auto rt = renderer.renderTargetForOutput(*edge);
        if(rt.renderTarget)
        {
          m_altPasses.emplace_back(
              edge, score::gfx::buildPipeline(
                        renderer, mesh, v, f, rt, m_processUBO, m_material.buffer,
                        m_samplers));
        }
      }
    }
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    auto& n = static_cast<const ImagesNode&>(this->node);

    if(n.tileMode != tile)
    {
      tile = n.tileMode;
      auto [s, tex] = m_samplers[0];

      m_samplers.clear();

      // Create a new sampler
      auto new_sampler = createSampler(tile, *renderer.state.rhi);
      m_samplers.push_back({new_sampler, tex});

      // Replace it in the render passes
      auto replace_sampler = [](PassMap& passes, QRhiSampler* oldS, QRhiSampler* newS) {
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

      auto replace_texture
          = [](PassMap& passes, QRhiSampler* sampler, QRhiTexture* tex) {
        for(auto& pass : passes)
          score::gfx::replaceTexture(*pass.second.srb, sampler, tex);
      };

      auto sampler = m_samplers[0].sampler;
      replace_texture(m_p, sampler, m_texture);
      replace_texture(m_altPasses, sampler, m_texture);
    }

    // If the current image being displayed by this renderer (in m_prev_ubo)
    // is out of date with the image in the data model, we switch the texture
    if(!m_uploaded || m_prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
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

    GenericNodeRenderer::update(renderer, res, edge);
  }

  void runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) override
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
#endif

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
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = vec2(texcoord.x, 1. - texcoord.y);
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto fullscreen_images_fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 factor = textureSize(y_tex, 0) / renderer.renderSize;
  vec2 ifactor = renderer.renderSize / textureSize(y_tex, 0);
  fragColor = texture(y_tex, v_texcoord);
}
)_";
FullScreenImageNode::FullScreenImageNode(QImage dec)
    : m_image{std::move(dec)}
{
  output.push_back(new Port{this, {}, Types::Image, {}});
}

FullScreenImageNode::~FullScreenImageNode() { }

class FullScreenImageNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~Renderer() { }

  TextureRenderTarget renderTargetForInput(const Port& p) override { return {}; }
  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    const auto& mesh = renderer.defaultTriangle();
    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);
    std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(
        renderer.state, fullscreen_images_vertex_shader,
        fullscreen_images_fragment_shader);

    auto& n = static_cast<const FullScreenImageNode&>(this->node);
    auto& rhi = *renderer.state.rhi;

    // Create GPU textures for the image
    const QSize sz = n.m_image.size();
    auto tex = rhi.newTexture(
        QRhiTexture::BGRA8, QSize{sz.width(), sz.height()}, 1, QRhiTexture::Flag{});

    tex->setName("FullScreenImageNode::tex");
    tex->create();
    m_texture = tex;

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("FullScreenImageNode::sampler");
      sampler->create();
      m_samplers.push_back({sampler, m_texture});
    }

    defaultPassesInit(renderer, mesh);
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* edge)
      override
  {
    GenericNodeRenderer::update(renderer, res, edge);

    auto& n = static_cast<const FullScreenImageNode&>(this->node);
    // If images haven't been uploaded yet, upload them.
    if(!m_uploaded)
    {
      res.uploadTexture(m_texture, renderer.adaptImage(n.m_image));
      m_uploaded = true;
    }
  }

  void runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) override
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

NodeRenderer* FullScreenImageNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

}
