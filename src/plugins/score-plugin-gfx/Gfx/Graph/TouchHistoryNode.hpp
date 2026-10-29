#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/PointerState.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QtCore/qfloat16.h>

#include <score_plugin_gfx_export.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <vector>

namespace score::gfx
{
/**
 * @brief The pointers of an output as a texture: vertexshaderart's `touch`
 *        history (greggman/vertexshaderart, src/js/main.js).
 *
 * 32 columns, one per pointer slot of @ref PointerState (0 = the mouse or the
 * first finger), 240 rows, one per rendered frame: texel (x, y, pressure,
 * time), x and y in clip space, y up. The newest row is the bottom of the
 * image -- v = 0 of IMG_NORM_PIXEL in an ISF and of texture() in a VSA, on
 * every backend -- and the top one is 239 frames old, as in WebGL where row 0
 * is v = 0 (main.js HistoryTexture / CPUHistoryTexture).
 *
 * The window devices publish it as the texture address `touch/history`, fed
 * from their `touch/points` address (Gfx/Window/PointerAddresses.hpp): it is an
 * ordinary texture source, read by whatever is bound or cabled to it.
 *
 * It draws into the render target of the input it feeds; that input should be
 * 32x240 RGBA32F, NEAREST, which is what the default binding of an image
 * input named `touch` asks for (ISFProcess.hpp). The draw is a straight copy:
 * the target is cleared to zero and the colour is composited as premultiplied,
 * so all four channels land as they are, time in alpha included.
 *
 * w is the seconds since this node was created, the same clock for every
 * output; on vertexshaderart it is the art's `time` uniform.
 */
struct SCORE_PLUGIN_GFX_TEST_EXPORT TouchHistoryNode : NodeModel
{
  static constexpr int columns = 32;
  static constexpr int rows = 240;

  explicit TouchHistoryNode(std::shared_ptr<PointerState> pointers)
      : pointers{std::move(pointers)}
  {
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
  ~TouchHistoryNode() override { m_materialData.release(); }

  //! What the history records every frame.
  std::shared_ptr<PointerState> pointers;
  const std::chrono::steady_clock::time_point epoch = std::chrono::steady_clock::now();

  NodeRenderer* createRenderer(RenderList& r) const noexcept override;
};

class SCORE_PLUGIN_GFX_EXPORT TouchHistoryRenderer final : public GenericNodeRenderer
{
public:
  explicit TouchHistoryRenderer(const TouchHistoryNode& node) noexcept
      : GenericNodeRenderer{node}
  {
    // See TouchHistoryNode: the four channels are copied as they are.
    m_outputPremultiplied = true;
  }

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void releaseState(RenderList& r) override;

  //! The history, newest row last (the bottom of the image), as uploaded.
  const std::vector<float>& history() const noexcept { return m_history; }

private:
  void upload(QRhiResourceUpdateBatch& res);

  QRhiTexture* m_texture{};
  bool m_half{};
  int64_t m_frame{-1};
  std::vector<float> m_history;
};

inline NodeRenderer* TouchHistoryNode::createRenderer(RenderList&) const noexcept
{
  return new TouchHistoryRenderer{*this};
}

namespace touch_history_shaders
{
static const constexpr auto vertex = R"_(#version 450
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
// The uploaded buffer's row 0 is the top of the image on every backend (the
// same correction as TexgenNode).
#if !(defined(QSHADER_HLSL) || defined(QSHADER_MSL))
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto fragment = R"_(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D history;

void main() { fragColor = texture(history, v_texcoord); }
)_";
}

inline void
TouchHistoryRenderer::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  m_mesh = &renderer.defaultTriangle();
  defaultMeshInit(renderer, *m_mesh, res);
  processUBOInit(renderer);
  m_material.init(renderer, node.input, m_samplers);
  std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(
      renderer.state, touch_history_shaders::vertex, touch_history_shaders::fragment);

  auto& rhi = *renderer.state.rhi;
  // main.js uses FLOAT where the device has it; half floats keep clip-space
  // positions and times well enough elsewhere.
  m_half = !rhi.isTextureFormatSupported(QRhiTexture::RGBA32F)
           && rhi.isTextureFormatSupported(QRhiTexture::RGBA16F);
  m_texture = rhi.newTexture(
      m_half ? QRhiTexture::RGBA16F : QRhiTexture::RGBA32F,
      QSize{TouchHistoryNode::columns, TouchHistoryNode::rows}, 1);
  m_texture->setName("TouchHistoryRenderer::m_texture");
  m_texture->create();

  // main.js: NEAREST, CLAMP_TO_EDGE.
  auto sampler = rhi.newSampler(
      QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
  sampler->setName("TouchHistoryRenderer::sampler");
  sampler->create();
  m_samplers.push_back({sampler, m_texture});

  m_history.assign(
      std::size_t(TouchHistoryNode::columns) * TouchHistoryNode::rows * 4, 0.f);
  m_frame = -1;
  upload(res);
  m_initialized = true;
}

inline void TouchHistoryRenderer::upload(QRhiResourceUpdateBatch& res)
{
  if(!m_texture)
    return;
  QByteArray bytes;
  if(m_half)
  {
    bytes.resize(qsizetype(m_history.size() * sizeof(qfloat16)));
    qFloatToFloat16(
        reinterpret_cast<qfloat16*>(bytes.data()), m_history.data(),
        qsizetype(m_history.size()));
  }
  else
  {
    bytes = QByteArray(
        reinterpret_cast<const char*>(m_history.data()),
        qsizetype(m_history.size() * sizeof(float)));
  }
  QRhiTextureSubresourceUploadDescription desc{bytes};
  res.uploadTexture(m_texture, QRhiTextureUploadDescription{{0, 0, desc}});
}

inline void
TouchHistoryRenderer::update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge*)
{
  defaultUBOUpdate(renderer, res);
  if(m_frame == renderer.frame)
    return;
  m_frame = renderer.frame;

  // One row per frame: the rows move up one, this frame's pointers go in the
  // bottom row, with the time in w for every column (main.js
  // updateSoundAndTouchHistory, then the history texture's update).
  auto& n = static_cast<const TouchHistoryNode&>(node);
  constexpr int rowFloats = TouchHistoryNode::columns * 4;
  constexpr int last = TouchHistoryNode::rows - 1;
  float* h = m_history.data();
  std::memmove(h, h + rowFloats, std::size_t(last) * rowFloats * sizeof(float));
  std::array<PointerState::Slot, PointerState::slots> slots{};
  if(n.pointers)
    slots = n.pointers->snapshot();
  const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - n.epoch)
                      .count();
  float* row = h + std::size_t(last) * rowFloats;
  for(int c = 0; c < TouchHistoryNode::columns; c++)
  {
    row[c * 4 + 0] = slots[c].x;
    row[c * 4 + 1] = slots[c].y;
    row[c * 4 + 2] = slots[c].pressure;
    row[c * 4 + 3] = t;
  }
  upload(res);
}

inline void TouchHistoryRenderer::releaseState(RenderList& r)
{
  if(m_texture)
    m_texture->deleteLater();
  m_texture = nullptr;
  m_history.clear();
  m_frame = -1;
  GenericNodeRenderer::releaseState(r);
}
}
