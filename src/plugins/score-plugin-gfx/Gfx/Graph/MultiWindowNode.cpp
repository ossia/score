#include <Gfx/Graph/MultiWindowNode.hpp>

#include <algorithm>
#include <cmath>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/OpenGL.hpp>

#include <QGuiApplication>
#include <QScreen>

namespace score::gfx
{

// Compute 3x3 homography matrix that maps the warped quad corners to the unit square.
// Given 4 source points (warped corners) and 4 destination points (unit square corners),
// this finds the inverse mapping: for screen position p, H*p gives the texture coordinate.
// Uses the standard DLT (Direct Linear Transform) approach.
static std::array<float, 12> computeHomographyUBO(const Gfx::CornerWarp& warp)
{
  // Source corners (the warped quad in output UV space)
  double sx0 = warp.topLeft.x(),     sy0 = warp.topLeft.y();
  double sx1 = warp.topRight.x(),    sy1 = warp.topRight.y();
  double sx2 = warp.bottomRight.x(), sy2 = warp.bottomRight.y();
  double sx3 = warp.bottomLeft.x(),  sy3 = warp.bottomLeft.y();

  // Destination: unit square corners (TL=0,0 TR=1,0 BR=1,1 BL=0,1)
  // We want the mapping FROM warped corners TO unit square,
  // i.e. for a screen pixel at position (u,v), compute where in [0,1]² to sample.
  // This is the inverse of the mapping unit_square -> warped_quad.

  // First compute the forward mapping: unit square -> warped quad
  // Using the standard 4-point perspective transform formulas
  double dx1 = sx1 - sx2, dy1 = sy1 - sy2;
  double dx2 = sx3 - sx2, dy2 = sy3 - sy2;
  double dx3 = sx0 - sx1 + sx2 - sx3, dy3 = sy0 - sy1 + sy2 - sy3;

  double denom = dx1 * dy2 - dx2 * dy1;
  if(std::abs(denom) < 1e-12)
  {
    // Degenerate: return identity
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  }

  double g = (dx3 * dy2 - dx2 * dy3) / denom;
  double h = (dx1 * dy3 - dx3 * dy1) / denom;

  double a = sx1 - sx0 + g * sx1;
  double b = sx3 - sx0 + h * sx3;
  double c = sx0;
  double d = sy1 - sy0 + g * sy1;
  double e = sy3 - sy0 + h * sy3;
  double f = sy0;
  // g, h already computed, i = 1

  // Forward matrix maps (u,v) in [0,1]² to warped position:
  //   [a b c] [u]     [x*w]
  //   [d e f] [v]  =  [y*w]
  //   [g h 1] [1]     [w  ]
  //
  // We need the INVERSE: maps warped position back to [0,1]²

  // Compute adjugate (transpose of cofactors) of 3x3 matrix
  double A = e * 1.0 - f * h;
  double B = -(b * 1.0 - c * h);
  double C = b * f - c * e;
  double D = -(d * 1.0 - f * g);
  double E = a * 1.0 - c * g;
  double F = -(a * f - c * d);
  double G = d * h - e * g;
  double H = -(a * h - b * g);
  double I = a * e - b * d;

  // Normalize so that I = 1 (or close to it)
  if(std::abs(I) > 1e-12)
  {
    double invI = 1.0 / I;
    A *= invI; B *= invI; C *= invI;
    D *= invI; E *= invI; F *= invI;
    G *= invI; H *= invI; I = 1.0;
  }

  // Pack into std140 format: 3 columns of vec4 (xyz used, w padding)
  // Column-major: col0 = first column of matrix
  // Matrix rows: row0 = [A, B, C], row1 = [D, E, F], row2 = [G, H, I]
  // In the shader we do dot(row, p), so we store rows as columns for the dot products
  return {
    (float)A, (float)B, (float)C, 0.0f,  // homographyCol0 (row 0)
    (float)D, (float)E, (float)F, 0.0f,  // homographyCol1 (row 1)
    (float)G, (float)H, (float)I, 0.0f   // homographyCol2 (row 2)
  };
}

// --- MultiWindowRenderer ---
// Renders the input texture's sub-regions to multiple swap chains.

class MultiWindowRenderer final : public score::gfx::OutputNodeRenderer
{
public:
  QShader m_vertexS, m_fragmentS;
  score::gfx::MeshBuffers m_mesh{};

  struct PerWindowData
  {
    score::gfx::Pipeline pipeline;
    QRhiBuffer* uvRectUBO{};
    QRhiBuffer* blendUBO{};
    QRhiBuffer* warpUBO{};
    std::array<score::gfx::Sampler, 1> samplers{};
    QRhiShaderResourceBindings* srb{};
    QRectF sourceRect{0, 0, 1, 1};
  };
  std::vector<PerWindowData> m_perWindow;

  const MultiWindowNode& m_multiNode;
  bool m_initialized{false};

  explicit MultiWindowRenderer(
      const MultiWindowNode& node, const score::gfx::RenderState& state)
      : score::gfx::OutputNodeRenderer{node}
      , m_multiNode{node}
  {
  }

  ~MultiWindowRenderer() override = default;

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    // The upstream graph renders into the node-owned offscreen target.
    // That target is always alive while the rhi is, independently of any
    // specific window.
    return m_multiNode.offscreenTarget();
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // Compile the sub-rect sampling shader
    // sourceRect is in canvas space (Y-down: y=0 top, y=1 bottom).
    // Following the ScaledRenderer pattern: on SPIRV flip v_texcoord.y,
    // on OpenGL flip sourceRect.y to convert from canvas Y-down to texture Y-up.
    static const constexpr auto frag_shader = R"_(#version 450
      layout(location = 0) in vec2 v_texcoord;
      layout(location = 0) out vec4 fragColor;

      layout(binding = 3) uniform sampler2D tex;

      layout(std140, binding = 2) uniform SubRect {
          vec4 sourceRect; // x, y, width, height in canvas UV space
      };

      layout(std140, binding = 4) uniform BlendParams {
          vec4 blendWidths; // left, right, top, bottom (in output UV 0-1)
          vec4 blendGammas; // left, right, top, bottom
      };

      layout(std140, binding = 5) uniform WarpParams {
          // mat3 stored as 3 x vec4 columns (std140 packing)
          vec4 homographyCol0;
          vec4 homographyCol1;
          vec4 homographyCol2;
          float warpEnabled;
          float rotationDeg;  // 0, 90, 180, 270
          float mirrorX;      // 0.0 or 1.0
          float mirrorY;      // 0.0 or 1.0
      };

      void main()
      {
          vec2 tc = v_texcoord;

          // Apply rotation + mirror transform (in screen UV space)
          {
              // Center at 0.5, 0.5
              vec2 c = tc - 0.5;

              // Rotation (clockwise in screen space)
              if(rotationDeg > 225.0)       // 270
                  c = vec2(c.y, -c.x);
              else if(rotationDeg > 135.0)  // 180
                  c = vec2(-c.x, -c.y);
              else if(rotationDeg > 45.0)   // 90
                  c = vec2(-c.y, c.x);

              // Mirror (after rotation)
              if(mirrorX > 0.5)
                  c.x = -c.x;
              if(mirrorY > 0.5)
                  c.y = -c.y;

              tc = c + 0.5;
          }

          // Apply inverse homography if corner warp is active
          if(warpEnabled > 0.5)
          {
              // Homography is computed in Y-down space (matching GUI).
              // v_texcoord.y is flipped, so convert to Y-down first.
              tc.y = 1.0 - tc.y;
              vec3 p = vec3(tc, 1.0);
              vec3 warped = vec3(
                  dot(vec3(homographyCol0.xyz), p),
                  dot(vec3(homographyCol1.xyz), p),
                  dot(vec3(homographyCol2.xyz), p)
              );
              tc = warped.xy / warped.z;

              // Discard pixels outside the warped quad
              if(tc.x < 0.0 || tc.x > 1.0 || tc.y < 0.0 || tc.y > 1.0)
              {
                  fragColor = vec4(0.0, 0.0, 0.0, 1.0);
                  return;
              }

              // Convert back to native texture coord space for sourceRect lookup
              tc.y = 1.0 - tc.y;
          }
          vec2 uv;
          uv.x = sourceRect.x + tc.x * sourceRect.z;
#if defined(QSHADER_SPIRV)
          uv.y = sourceRect.y + (1.0 - tc.y) * sourceRect.w;
#else
          float texSrcY = 1.0 - sourceRect.y - sourceRect.w;
          uv.y = texSrcY + tc.y * sourceRect.w;
#endif

          if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
          {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
          }
          else
          {
            fragColor = texture(tex, uv);
          }
          // Soft-edge blending
          // v_texcoord.y is Y-up on both GL and Vulkan (Qt RHI normalizes this),
          // so tc.y=0 is screen bottom and tc.y=1 is screen top.
          float alpha = 1.0;
          // Left edge
          if(blendWidths.x > 0.0 && tc.x < blendWidths.x)
              alpha *= pow(tc.x / blendWidths.x, blendGammas.x);
          // Right edge
          if(blendWidths.y > 0.0 && tc.x > 1.0 - blendWidths.y)
              alpha *= pow((1.0 - tc.x) / blendWidths.y, blendGammas.y);
          // Top edge (tc.y near 1.0 = screen top)
          if(blendWidths.z > 0.0 && tc.y > 1.0 - blendWidths.z)
              alpha *= pow((1.0 - tc.y) / blendWidths.z, blendGammas.z);
          // Bottom edge (tc.y near 0.0 = screen bottom)
          if(blendWidths.w > 0.0 && tc.y < blendWidths.w)
              alpha *= pow(tc.y / blendWidths.w, blendGammas.w);

          fragColor = vec4(fragColor.rgb * alpha, alpha);
      }
      )_";

    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), frag_shader);

    // Reserve per-window slots. Pipelines are built lazily below for
    // windows that already have a swap chain; others will be built when
    // their swap chain becomes ready (via buildWindowPipeline()).
    const auto& outputs = m_multiNode.windowOutputs();
    m_perWindow.resize(outputs.size());

    for(int i = 0; i < (int)outputs.size(); ++i)
      buildWindowPipeline(i, renderer, &res);

    m_initialized = true;
  }

  // Build (or rebuild) the per-window GPU resources for a single window.
  // Safe to call multiple times: an existing pipeline is released first.
  // `res` may be null; UBO contents are refreshed every frame inside
  // renderSubRegion() so we don't need to perform an initial upload here.
  void buildWindowPipeline(
      int index, score::gfx::RenderList& renderer, QRhiResourceUpdateBatch* res)
  {
    if(index < 0 || index >= (int)m_perWindow.size())
      return;

    auto& rhi = *renderer.state.rhi;
    const auto& outputs = m_multiNode.windowOutputs();
    if(index >= (int)outputs.size())
      return;

    const auto& wo = outputs[index];
    auto& pw = m_perWindow[index];

    // A window without an RPD has no swap chain yet; postpone build.
    if(!wo.renderPassDescriptor)
      return;
    // The offscreen target must exist — the sampler binds to its texture.
    if(!m_multiNode.offscreenTarget().texture)
      return;

    // Release any existing pipeline/resources for this window before
    // rebuilding so the method is idempotent on re-expose.
    releaseWindowPipeline(index);

    pw.sourceRect = wo.sourceRect;

    // Create UBO for source rect
    pw.uvRectUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 4 * sizeof(float));
    pw.uvRectUBO->setName(
        QByteArray("MultiWindowRenderer::uvRectUBO_") + QByteArray::number(index));
    pw.uvRectUBO->create();

    // Create UBO for blend parameters (4 widths + 4 gammas = 8 floats)
    pw.blendUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 8 * sizeof(float));
    pw.blendUBO->setName(
        QByteArray("MultiWindowRenderer::blendUBO_") + QByteArray::number(index));
    pw.blendUBO->create();

    // Create UBO for corner warp (3 vec4 columns + 4 floats trailing)
    pw.warpUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16 * sizeof(float));
    pw.warpUBO->setName(
        QByteArray("MultiWindowRenderer::warpUBO_") + QByteArray::number(index));
    pw.warpUBO->create();

    // Seed initial UBO contents if we were given a batch to piggy-back on.
    // When `res` is null (lazy build from initWindowSwapChain) we skip the
    // initial upload; renderSubRegion() re-uploads every frame anyway.
    if(res)
    {
      float rectData[4] = {
          (float)pw.sourceRect.x(), (float)pw.sourceRect.y(),
          (float)pw.sourceRect.width(), (float)pw.sourceRect.height()};
      res->updateDynamicBuffer(pw.uvRectUBO, 0, sizeof(rectData), rectData);

      float blendData[8] = {
          wo.blendLeft.width,  wo.blendRight.width, wo.blendTop.width,
          wo.blendBottom.width, wo.blendLeft.gamma, wo.blendRight.gamma,
          wo.blendTop.gamma,   wo.blendBottom.gamma};
      res->updateDynamicBuffer(pw.blendUBO, 0, sizeof(blendData), blendData);

      float warpEnabled = wo.cornerWarp.isIdentity() ? 0.0f : 1.0f;
      auto hom = computeHomographyUBO(wo.cornerWarp);
      float warpData[16] = {};
      std::copy(hom.begin(), hom.end(), warpData);
      warpData[12] = warpEnabled;
      warpData[13] = (float)wo.rotation;
      warpData[14] = wo.mirrorX ? 1.0f : 0.0f;
      warpData[15] = wo.mirrorY ? 1.0f : 0.0f;
      res->updateDynamicBuffer(pw.warpUBO, 0, sizeof(warpData), warpData);
    }

    // Create sampler reading from the node's offscreen target.
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->setName("MultiWindowRenderer::sampler");
      sampler->create();

      pw.samplers[0] = {sampler, m_multiNode.offscreenTarget().texture};
    }

    // Build the blit pipeline against this window's render pass descriptor.
    {
      score::gfx::TextureRenderTarget rt;
      rt.renderPass = wo.renderPassDescriptor;

      QRhiShaderResourceBinding extraBindings[2] = {
          QRhiShaderResourceBinding::uniformBuffer(
              4, QRhiShaderResourceBinding::FragmentStage, pw.blendUBO),
          QRhiShaderResourceBinding::uniformBuffer(
              5, QRhiShaderResourceBinding::FragmentStage, pw.warpUBO)};

      const auto& mesh = renderer.defaultTriangle();
      pw.pipeline = score::gfx::buildPipeline(
          renderer, mesh, m_vertexS, m_fragmentS, rt, nullptr, pw.uvRectUBO,
          pw.samplers, {extraBindings, 2});
    }
  }

  void releaseWindowPipeline(int index)
  {
    if(index < 0 || index >= (int)m_perWindow.size())
      return;

    auto& pw = m_perWindow[index];
    pw.pipeline.release();
    for(auto& s : pw.samplers)
      delete s.sampler;
    pw.samplers = {};
    delete pw.uvRectUBO;
    pw.uvRectUBO = nullptr;
    delete pw.blendUBO;
    pw.blendUBO = nullptr;
    delete pw.warpUBO;
    pw.warpUBO = nullptr;
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    for(int i = 0; i < (int)m_perWindow.size(); ++i)
      releaseWindowPipeline(i);
    m_perWindow.clear();
    m_initialized = false;
    // m_offscreenTarget is owned by MultiWindowNode; do NOT release it here.
  }

  // Sub-region 0 is no longer special — it's blitted in its own per-window
  // frame just like all other windows. finishFrame is a no-op so that
  // rl->render(cb) can be invoked inside an offscreen frame that is not
  // attached to any swap chain.
  void finishFrame(
      score::gfx::RenderList& /*renderer*/, QRhiCommandBuffer& /*cb*/,
      QRhiResourceUpdateBatch*& /*res*/) override
  {
  }

  // Called by MultiWindowNode::render() for each live window.
  void renderToWindow(
      int windowIndex, score::gfx::RenderList& renderer, QRhiCommandBuffer& cb)
  {
    if(windowIndex < 0 || windowIndex >= (int)m_perWindow.size())
      return;

    auto* res = renderer.state.rhi->nextResourceUpdateBatch();
    renderSubRegion(windowIndex, renderer, cb, res);
  }

private:
  void renderSubRegion(
      int index, score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res)
  {
    const auto& outputs = m_multiNode.windowOutputs();
    if(index >= (int)outputs.size())
      return;

    const auto& wo = outputs[index];
    auto& pw = m_perWindow[index];

    if(!wo.swapChain || !wo.hasSwapChain)
      return;

    // Skip windows whose pipeline hasn't been built yet (e.g. window
    // became ready after the initial renderer init and we haven't had a
    // chance to lazily build it).
    if(!pw.pipeline.pipeline || !pw.pipeline.srb)
      return;

    auto rt = wo.swapChain->currentFrameRenderTarget();
    if(!rt)
      return;

    // Update UBOs from live data (may have changed via device parameters)
    if(pw.uvRectUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();

      float rectData[4] = {
          (float)wo.sourceRect.x(), (float)wo.sourceRect.y(),
          (float)wo.sourceRect.width(), (float)wo.sourceRect.height()};
      res->updateDynamicBuffer(pw.uvRectUBO, 0, sizeof(rectData), rectData);
    }

    if(pw.blendUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();

      float blendData[8] = {
          wo.blendLeft.width, wo.blendRight.width, wo.blendTop.width, wo.blendBottom.width,
          wo.blendLeft.gamma, wo.blendRight.gamma, wo.blendTop.gamma, wo.blendBottom.gamma};
      res->updateDynamicBuffer(pw.blendUBO, 0, sizeof(blendData), blendData);
    }

    if(pw.warpUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();

      float warpEnabled = wo.cornerWarp.isIdentity() ? 0.0f : 1.0f;
      auto hom = computeHomographyUBO(wo.cornerWarp);
      float warpData[16] = {};
      std::copy(hom.begin(), hom.end(), warpData);
      warpData[12] = warpEnabled;
      warpData[13] = (float)wo.rotation;
      warpData[14] = wo.mirrorX ? 1.0f : 0.0f;
      warpData[15] = wo.mirrorY ? 1.0f : 0.0f;
      res->updateDynamicBuffer(pw.warpUBO, 0, sizeof(warpData), warpData);
    }

    cb.beginPass(rt, Qt::black, {1.0f, 0}, res);
    res = nullptr;
    {
      auto sz = wo.swapChain->currentPixelSize();
      cb.setGraphicsPipeline(pw.pipeline.pipeline);
      cb.setShaderResources(pw.pipeline.srb);
      cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

      const auto& mesh = renderer.defaultTriangle();
      mesh.draw(m_mesh, cb);
    }
    cb.endPass();
  }
};

// --- MultiWindowNode implementation ---

MultiWindowNode::MultiWindowNode(
    Configuration conf, const std::vector<Gfx::OutputMapping>& mappings)
    : OutputNode{}
    , m_conf{conf}
    , m_mappings{mappings}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

MultiWindowNode::~MultiWindowNode()
{
  // Delegate to destroyOutput() to ensure a single, consistent cleanup path.
  // destroyOutput() is idempotent so calling it here after Graph::~Graph
  // already called it is safe.
  destroyOutput();
}

bool MultiWindowNode::canRender() const
{
  // With offscreen-first rendering we don't need any window to be alive:
  // the upstream graph renders into the offscreen target regardless.
  // Per-window blitting skips dead windows individually.
  return m_renderState && m_renderState->rhi
         && m_offscreenTarget.renderPass != nullptr;
}

void MultiWindowNode::setRenderSize(QSize sz)
{
  if(!m_renderState)
    return;
  if(sz.width() < 1 || sz.height() < 1)
    return;
  if(sz == m_renderState->renderSize)
    return;

  m_renderState->renderSize = sz;

  // The offscreen target must be recreated BEFORE the render-list
  // rebuild so that the new upstream pipelines are built against the
  // new RPD and sample from the new offscreen texture. The old
  // pipelines briefly reference the deleted RPD, but their destruction
  // (inside the upcoming m_onResize) doesn't dereference it.
  recreateOffscreenTarget();

  if(m_onResize)
    m_onResize();
}

void MultiWindowNode::setSourceRect(int windowIndex, QRectF rect)
{
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
    m_windowOutputs[windowIndex].sourceRect = rect;
}

void MultiWindowNode::setEdgeBlend(int windowIndex, int side, float width, float gamma)
{
  if(windowIndex < 0 || windowIndex >= (int)m_windowOutputs.size())
    return;

  auto& wo = m_windowOutputs[windowIndex];
  EdgeBlendData* target{};
  switch(side)
  {
    case 0: target = &wo.blendLeft; break;
    case 1: target = &wo.blendRight; break;
    case 2: target = &wo.blendTop; break;
    case 3: target = &wo.blendBottom; break;
    default: return;
  }
  target->width = width;
  target->gamma = gamma;
}

void MultiWindowNode::setCornerWarp(int windowIndex, const Gfx::CornerWarp& warp)
{
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
    m_windowOutputs[windowIndex].cornerWarp = warp;
}

void MultiWindowNode::setTransform(int windowIndex, int rotation, bool mirrorX, bool mirrorY)
{
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
  {
    auto& wo = m_windowOutputs[windowIndex];
    wo.rotation = rotation;
    wo.mirrorX = mirrorX;
    wo.mirrorY = mirrorY;
  }
}

void MultiWindowNode::setSwapchainFlag(Gfx::SwapchainFlag flag)
{
  m_swapchainFlag = flag;
}

void MultiWindowNode::setSwapchainFormat(Gfx::SwapchainFormat format)
{
  m_swapchainFormat = format;
}

void MultiWindowNode::startRendering()
{
  if(onFps)
    onFps(0.f);
}

void MultiWindowNode::renderBlack()
{
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto rhi = m_renderState->rhi;

  for(auto& wo : m_windowOutputs)
  {
    if(!wo.window || !wo.swapChain || !wo.hasSwapChain)
      continue;

    if(wo.swapChain->currentPixelSize() != wo.swapChain->surfacePixelSize())
      wo.hasSwapChain = wo.swapChain->createOrResize();

    if(!wo.hasSwapChain)
      continue;

    QRhi::FrameOpResult r = rhi->beginFrame(wo.swapChain);
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(!wo.hasSwapChain)
        continue;
      r = rhi->beginFrame(wo.swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
      continue;

    auto cb = wo.swapChain->currentFrameCommandBuffer();
    auto batch = rhi->nextResourceUpdateBatch();
    cb->beginPass(wo.swapChain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, batch);
    cb->endPass();

    rhi->endFrame(wo.swapChain);
  }
}

void MultiWindowNode::render()
{
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto rhi = m_renderState->rhi;

  // Handle swap chain resizes for all live windows. Dead (closed) windows
  // have wo.swapChain == nullptr and are skipped naturally.
  for(auto& wo : m_windowOutputs)
  {
    if(!wo.window || !wo.swapChain)
      continue;
    if(wo.swapChain->currentPixelSize() != wo.swapChain->surfacePixelSize())
      wo.hasSwapChain = wo.swapChain->createOrResize();
  }

  auto rl = m_renderer.lock();
  if(!rl || rl->renderers.size() <= 1)
  {
    // No active render graph — just clear every live window to black.
    renderBlack();
    return;
  }

  // Phase 1: render the upstream graph into the offscreen target, in a
  // frame that is not attached to any swap chain. This is what decouples
  // upstream rendering from any specific window's lifetime.
  {
    QRhiCommandBuffer* cb = nullptr;
    QRhi::FrameOpResult r = rhi->beginOffscreenFrame(&cb);
    if(r != QRhi::FrameOpSuccess || !cb)
      return;

    rl->render(*cb);

    rhi->endOffscreenFrame();
  }

  // Phase 2: for each live window, blit its sub-region in its own frame.
  // Any window whose swap chain is gone or out-of-date is skipped without
  // affecting the others.
  if(this->renderedNodes.empty())
    return;
  auto outRenderer
      = dynamic_cast<MultiWindowRenderer*>(this->renderedNodes.begin()->second);
  if(!outRenderer)
    return;

  for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    if(!wo.window || !wo.swapChain || !wo.hasSwapChain)
      continue;

    QRhi::FrameOpResult r = rhi->beginFrame(wo.swapChain);
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(!wo.hasSwapChain)
        continue;
      r = rhi->beginFrame(wo.swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
      continue;

    auto cb = wo.swapChain->currentFrameCommandBuffer();
    outRenderer->renderToWindow(i, *rl, *cb);

    rhi->endFrame(wo.swapChain);
  }
}

void MultiWindowNode::onRendererChange()
{
}

void MultiWindowNode::stopRendering()
{
  if(onFps)
    onFps(0.f);
}

void MultiWindowNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_renderer = r;
}

RenderList* MultiWindowNode::renderer() const
{
  return m_renderer.lock().get();
}

void MultiWindowNode::recreateOffscreenTarget()
{
  if(!m_renderState || !m_renderState->rhi)
    return;

  // Tear down any previous offscreen target, including its RPD. Any
  // pipelines built against this RPD must have been released already;
  // the callers of this method are responsible for that (createOutput,
  // destroyOutput, and the renderSize-change path which triggers a full
  // render-list rebuild).
  m_offscreenTarget.release();

  QSize sz = m_renderState->renderSize;
  if(sz.width() <= 0 || sz.height() <= 0)
    sz = QSize{1280, 720};

  m_offscreenTarget = score::gfx::createRenderTarget(
      *m_renderState, m_renderState->renderFormat, sz, m_renderState->samples,
      /*depth*/ true);

  // Publish the RPD so that any code path that reads
  // state.renderPassDescriptor (ScaledRenderer-style renderers) sees a
  // stable, window-independent value. Not strictly required by
  // MultiWindowRenderer (which uses per-window RPDs for its blit
  // pipelines) but it keeps the contract of RenderState consistent with
  // other output nodes.
  m_renderState->renderPassDescriptor = m_offscreenTarget.renderPass;
}

void MultiWindowNode::initWindowSwapChain(int index)
{
  if(index < 0 || index >= (int)m_windowOutputs.size())
    return;
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto& wo = m_windowOutputs[index];
  if(!wo.window)
    return;

  // Release any existing swap chain for this window first. This makes
  // initWindowSwapChain idempotent so it can be called again when a
  // closed window is re-exposed.
  releaseWindowSwapChain(index);

  auto& mapping = m_mappings[index];
  auto rhi = m_renderState->rhi;

  wo.swapChain = rhi->newSwapChain();
  wo.swapChain->setWindow(wo.window.get());
#if QT_VERSION > QT_VERSION_CHECK(6, 4, 0)
  wo.swapChain->setFormat((QRhiSwapChain::Format)m_swapchainFormat);
#endif

  wo.depthStencil = rhi->newRenderBuffer(
      QRhiRenderBuffer::DepthStencil, QSize(), m_renderState->samples,
      QRhiRenderBuffer::UsedWithSwapChainOnly);
  wo.swapChain->setDepthStencil(wo.depthStencil);
  wo.swapChain->setSampleCount(m_renderState->samples);

  QRhiSwapChain::Flags flags = QRhiSwapChain::MinimalBufferCount;
  // With multi-window we drive rendering from the node's timer and don't
  // rely on swap-chain vsync. Keep NoVSync on every window so no single
  // swap chain blocks the others.
  flags |= QRhiSwapChain::NoVSync;
  if(m_swapchainFlag == Gfx::SwapchainFlag::sRGB)
    flags |= QRhiSwapChain::sRGB;
  wo.swapChain->setFlags(flags);

  wo.renderPassDescriptor = wo.swapChain->newCompatibleRenderPassDescriptor();
  wo.swapChain->setRenderPassDescriptor(wo.renderPassDescriptor);

  // Note: mapping-derived fields (sourceRect, blend*, cornerWarp,
  // rotation, mirror*) are seeded once from `m_mappings[index]` in
  // createOutput(). They are deliberately not touched here so that
  // live parameter changes survive a close + re-expose cycle.
  (void)mapping;

  wo.hasSwapChain = wo.swapChain->createOrResize();
  if(wo.hasSwapChain)
  {
    if(m_renderState->outputSize.isEmpty())
      m_renderState->outputSize = wo.swapChain->currentPixelSize();
  }

  // If a renderer already exists, lazily build the per-window pipeline
  // for this newly-ready window so the next frame can blit to it.
  if(auto rl = m_renderer.lock())
  {
    if(auto it = this->renderedNodes.find(rl.get());
       it != this->renderedNodes.end())
    {
      if(auto* mwr = dynamic_cast<MultiWindowRenderer*>(it->second))
      {
        mwr->buildWindowPipeline(index, *rl, nullptr);
      }
    }
  }
}

void MultiWindowNode::releaseWindowSwapChain(int index)
{
  if(index < 0 || index >= (int)m_windowOutputs.size())
    return;
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto& wo = m_windowOutputs[index];
  if(!wo.swapChain && !wo.depthStencil && !wo.renderPassDescriptor)
    return;

  // Wait for any in-flight frames touching this swap chain before tearing
  // its resources down.
  m_renderState->rhi->finish();

  // Release the renderer's per-window GPU state first, so its pipeline
  // (built against wo.renderPassDescriptor) is gone before we delete the
  // RPD itself.
  if(auto rl = m_renderer.lock())
  {
    if(auto it = this->renderedNodes.find(rl.get());
       it != this->renderedNodes.end())
    {
      if(auto* mwr = dynamic_cast<MultiWindowRenderer*>(it->second))
      {
        mwr->releaseWindowPipeline(index);
      }
    }
  }

  delete wo.swapChain;
  wo.swapChain = nullptr;

  delete wo.depthStencil;
  wo.depthStencil = nullptr;

  delete wo.renderPassDescriptor;
  wo.renderPassDescriptor = nullptr;

  wo.hasSwapChain = false;
}

void MultiWindowNode::createOutput(score::gfx::OutputConfiguration conf)
{
  if(m_mappings.empty())
    return;

  // Create shared QRhi without a specific window. The rhi will be used
  // by every window's swap chain as well as by the offscreen target.
  m_renderState
      = score::gfx::createRenderState(conf.graphicsApi, QSize{1280, 720}, nullptr);
  if(!m_renderState || !m_renderState->rhi)
    return;

  m_renderState->renderFormat = (m_swapchainFormat != Gfx::SwapchainFormat::SDR)
                                    ? QRhiTexture::RGBA32F
                                    : QRhiTexture::RGBA8;
  m_renderState->outputSize = m_renderState->renderSize;

  // Stash onResize so per-window events (re-expose, close+reopen) can
  // request a full render-list rebuild when they need one.
  m_onResize = conf.onResize;

  // 1. Create the offscreen target that is the stable render target
  //    for the upstream graph. This must exist BEFORE the render list is
  //    built because the renderer queries offscreenTarget() in its init.
  recreateOffscreenTarget();

  m_windowOutputs.resize(m_mappings.size());

  // 2. Create every window. Their swap chains will be created lazily
  //    per-window in onWindowReady as each platform surface becomes
  //    available — we no longer wait on window 0 to drive all of them.
  for(int i = 0; i < (int)m_mappings.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    auto& mapping = m_mappings[i];

    // Seed the live per-window state from the mapping exactly once.
    // Parameter callbacks mutate these fields afterwards, and we want
    // those mutations to survive a close+re-expose cycle, so
    // initWindowSwapChain() never touches them.
    wo.sourceRect = mapping.sourceRect;
    wo.blendLeft = {mapping.blendLeft.width, mapping.blendLeft.gamma};
    wo.blendRight = {mapping.blendRight.width, mapping.blendRight.gamma};
    wo.blendTop = {mapping.blendTop.width, mapping.blendTop.gamma};
    wo.blendBottom = {mapping.blendBottom.width, mapping.blendBottom.gamma};
    wo.cornerWarp = mapping.cornerWarp;
    wo.rotation = mapping.rotation;
    wo.mirrorX = mapping.mirrorX;
    wo.mirrorY = mapping.mirrorY;

    wo.window = std::make_shared<Window>(conf.graphicsApi);
    wo.window->setTitle(QString("Output %1").arg(i));

    // Each window gets its own onWindowReady callback. When the native
    // surface becomes ready (first expose, or re-expose after a close),
    // we create / recreate just this window's swap chain and lazily
    // build its blit pipeline.
    wo.window->onWindowReady = [this, i] { initWindowSwapChain(i); };

    // When the user closes the window (or the platform destroys the
    // surface), tear down that window's swap chain so the other windows
    // keep running. The QWindow itself stays alive so the user can
    // later re-show it via the ossia parameters.
    wo.window->onClose = [this, i] { releaseWindowSwapChain(i); };

    // A per-window resize event only needs to poke its own swap chain.
    // The upstream graph's size is governed by the offscreen target and
    // by setRenderSize(), independently of any window.
    wo.window->onResize = [this, i] {
      if(i >= (int)m_windowOutputs.size())
        return;
      auto& w = m_windowOutputs[i];
      if(w.swapChain)
        w.hasSwapChain = w.swapChain->createOrResize();
    };

    // Determine target screen
    QScreen* targetScreen = nullptr;
    if(mapping.screenIndex >= 0)
    {
      const auto& screens = qApp->screens();
      if(mapping.screenIndex < screens.size())
        targetScreen = screens[mapping.screenIndex];
    }

    if(mapping.fullscreen)
    {
      if(targetScreen)
      {
        wo.window->setScreen(targetScreen);
        wo.window->setGeometry(targetScreen->geometry());
      }
      wo.window->showFullScreen();
    }
    else
    {
      if(targetScreen)
        wo.window->setScreen(targetScreen);
      wo.window->setGeometry(QRect(mapping.windowPosition, mapping.windowSize));
      wo.window->show();
    }
  }

  // Notify the device that the QWindow objects exist (it uses this to
  // connect Qt signals like mouseMove, key, etc).
  if(onWindowsCreated)
    onWindowsCreated();

  // 3. The graph can build the render list right now — it only needs
  //    rhi + offscreen target, both of which are ready synchronously.
  //    This means the upstream graph renders from the very first frame,
  //    regardless of whether any window has been exposed yet.
  if(conf.onReady)
    conf.onReady();
}

void MultiWindowNode::destroyOutput()
{
  // Nothing to clean up if the rhi was never successfully created.
  if(!m_renderState || !m_renderState->rhi)
  {
    // Still release any windows that may have been created before init failed.
    m_windowOutputs.clear();
    m_offscreenTarget = {};
    if(m_renderState)
      m_renderState.reset();
    return;
  }

  // Ensure the GPU has no outstanding work on any of our swap chains before
  // we start tearing resources down. Without this, the validation layer may
  // report leaked VkBuffer / VkCommandBuffer / VkInstance at shutdown because
  // there are still frames in flight when resources are destroyed.
  m_renderState->rhi->finish();

  // Detach Window callbacks so a close that races with destruction can't
  // reach back into us while we're tearing things down.
  for(auto& wo : m_windowOutputs)
  {
    if(wo.window)
    {
      wo.window->onWindowReady = {};
      wo.window->onClose = {};
      wo.window->onResize = {};
    }
  }

  // 1. Destroy per-window GPU resources (swap chain, depth, RPD) first.
  //    Keep the QWindow shared_ptrs alive — their VkSurfaceKHR must
  //    outlive the rhi's teardown of per-window state.
  for(auto& wo : m_windowOutputs)
  {
    delete wo.swapChain;
    wo.swapChain = nullptr;

    delete wo.depthStencil;
    wo.depthStencil = nullptr;

    delete wo.renderPassDescriptor;
    wo.renderPassDescriptor = nullptr;

    wo.hasSwapChain = false;
  }

  // 2. Release the offscreen target (texture + depth + RT + RPD). This
  //    must happen before `destroy()` on the rhi. It's also where
  //    m_renderState->renderPassDescriptor is cleared (we aliased the
  //    pointer in recreateOffscreenTarget()).
  m_offscreenTarget.release();
  m_renderState->renderPassDescriptor = nullptr;

  // 3. Tear down the rhi. Must happen while the windows (and thus
  //    their VkSurfaceKHR) are still alive.
  m_renderState->destroy();

  // 4. Now that the rhi is gone, releasing the windows / their surfaces
  //    is safe. Clearing the vector destroys each WindowOutput which in
  //    turn releases its shared_ptr<Window>.
  m_windowOutputs.clear();

  // 5. Release our reference to the now-empty RenderState so we don't
  //    touch it again.
  m_renderState.reset();
}

void MultiWindowNode::updateGraphicsAPI(GraphicsApi api)
{
  if(!m_renderState)
    return;

  if(m_renderState->api != api)
    destroyOutput();
}

void MultiWindowNode::setVSyncCallback(std::function<void()> f)
{
  m_vsyncCallback = std::move(f);
}

std::shared_ptr<RenderState> MultiWindowNode::renderState() const
{
  return m_renderState;
}

OutputNodeRenderer* MultiWindowNode::createRenderer(RenderList& r) const noexcept
{
  return new MultiWindowRenderer{*this, r.state};
}

OutputNode::Configuration MultiWindowNode::configuration() const noexcept
{
  return m_conf;
}

}
