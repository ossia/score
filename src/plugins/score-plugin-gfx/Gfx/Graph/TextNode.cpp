#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/TextNode.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QPainter>

namespace score::gfx
{

static const constexpr auto text_vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding = 3) uniform sampler2D y_tex;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  float opacity;
  vec2 position;
  vec2 scale;
} mat;
out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(mat.position + mat.scale * position, 0.0, 1.);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto text_fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
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
TextNode::TextNode()
{
  // FIXME why are the others missing ?????
  input.push_back(new Port{this, &ubo.opacity, Types::Float, {}});
  input.push_back(new Port{this, &ubo.position[0], Types::Vec2, {}});
  input.push_back(new Port{this, &ubo.scale[0], Types::Vec2, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});

  m_materialData.reset((char*)&ubo);
}

TextNode::~TextNode()
{
  m_materialData.release();
}

class TextNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

private:
  ~Renderer() { }

  TextureRenderTarget renderTargetForInput(const Port& p) override { return {}; }

  // TODO
  QSize sz{1920, 1080};
  void rerender()
  {
    auto& n = static_cast<const TextNode&>(this->node);

    if(m_img.size().isNull())
      m_img = QImage(sz, QImage::Format::Format_ARGB32_Premultiplied);
    m_img.fill(Qt::transparent);
    {
      QPainter p{&m_img};
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);

      p.setFont(n.font);
      p.setPen(n.pen);
      p.drawText(10, 10, sz.width() - 20, sz.height() - 20, 0, n.text);
    }

    m_uploaded = false;
  }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    rerender();
    const auto& mesh = renderer.defaultQuad();
    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);
    std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(
        renderer.state, text_vertex_shader, text_fragment_shader);

    QRhi& rhi = *renderer.state.rhi;

    {
      auto tex = rhi.newTexture(QRhiTexture::BGRA8, sz, 1, QRhiTexture::Flag{});

      tex->setName("TextNode::tex");
      tex->create();
      m_textures.push_back({{}, tex});
    }

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("TextNode::sampler");
      sampler->create();
      auto tex
          = m_textures.empty() ? &renderer.emptyTexture() : m_textures.front().second;
      m_samplers.push_back({sampler, tex});
    }

    defaultPassesInit(renderer, mesh);
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    defaultUBOUpdate(renderer, res);

    if(m_textures.empty())
      return;

    // If images haven't been uploaded yet, upload them.

    auto& n = static_cast<const TextNode&>(this->node);
    if(n.mustRerender)
    {
      rerender();
    }

    if(!m_uploaded)
    {
      res.uploadTexture(m_textures[0].second, m_img);

      m_uploaded = true;
    }
  }

  void runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    defaultRenderPass(renderer, mesh, cb, edge);
  }

  void release(RenderList& r) override
  {
    for(auto tex : m_textures)
    {
      tex.second->deleteLater();
    }
    m_textures.clear();

    defaultRelease(r);
  }

  QImage m_img;
  std::vector<std::pair<score::gfx::Edge*, QRhiTexture*>> m_textures;
  bool m_uploaded = false;
};

NodeRenderer* TextNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

void TextNode::process(Message&& msg)
{
  ProcessNode::process(msg.token);

  int32_t p = 0;
  for(const gfx_input& m : msg.input)
  {
    if(auto val = ossia::get_if<ossia::value>(&m))
    {
      switch(p)
      {
        case 0: {
          // Text
          {
            text = QString::fromStdString(ossia::convert<std::string>(*val));
            mustRerender = true;
          }
          break;
        }
        case 1: {
          // Font
          {
            font.setFamily(QString::fromStdString(ossia::convert<std::string>(*val)));
            mustRerender = true;
          }
          break;
        }
        case 2: {
          // Point size
          {
            font.setPointSizeF(ossia::convert<float>(*val));
            mustRerender = true;
          }
          break;
        }

        case 3: // Opacity
        {
          auto opacity = ossia::convert<float>(*val);
          this->ubo.opacity = ossia::clamp(opacity, 0.f, 1.f);
          this->materialChange();
          break;
        }
        case 4: // Position
        {
          auto sink = ossia::gfx::port_index{msg.node_id, p - 3};
          ossia::visit(
              [this, sink](const auto& v) { ProcessNode::process(sink.port, v); },
              std::move(m));
          break;
        }

        case 5: // Scale X
        {
          {
            auto scale = ossia::convert<float>(*val);
            this->ubo.scale[0] = scale;
            this->materialChange();
          }
          break;
        }
        case 6: // Scale Y
        {
          {
            auto scale = ossia::convert<float>(*val);
            this->ubo.scale[1] = scale;
            this->materialChange();
          }
          break;
        }

        case 7: {
          // Color
          {
            auto rgba = ossia::convert<ossia::vec4f>(*val);
            pen.setColor(QColor::fromRgbF(rgba[0], rgba[1], rgba[2], rgba[3]));
            mustRerender = true;
          }
          break;
        }
      }
    }

    p++;
  }
}

}
