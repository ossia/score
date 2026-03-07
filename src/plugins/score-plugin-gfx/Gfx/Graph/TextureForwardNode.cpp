#include <Gfx/Graph/TextureForwardNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

namespace score::gfx
{

static const constexpr auto texture_forward_vertex = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";

static const constexpr auto texture_forward_fragment = R"_(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;

void main()
{
  fragColor = texture(y_tex, v_texcoord);
}
)_";

class TextureForwardRenderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;
  ~TextureForwardRenderer() { }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    const auto& mesh = renderer.defaultTriangle();
    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, texture_forward_vertex, texture_forward_fragment);
    defaultPassesInit(renderer, mesh);
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    defaultUBOUpdate(renderer, res);
  }

  void release(RenderList& r) override
  {
    defaultRelease(r);
  }
};

TextureForwardNode::TextureForwardNode()
{
  // 1 texture input, 1 texture output
  input.push_back(new Port{this, {}, Types::Image, {}, {}});
  output.push_back(new Port{this, {}, Types::Image, {}, {}});
}

TextureForwardNode::~TextureForwardNode() { }

NodeRenderer* TextureForwardNode::createRenderer(RenderList& r) const noexcept
{
  return new TextureForwardRenderer{*this};
}

void TextureForwardNode::process(Message&& msg)
{
  NodeModel::process(std::move(msg));
}

}
