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
  // The input target where all upstream nodes render to
  score::gfx::TextureRenderTarget m_inputTarget;

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
    return m_inputTarget;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;

    // Create the input target where upstream nodes will render
    m_inputTarget = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::Format::RGBA8, renderer.state.renderSize,
        renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

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
      };

      void main()
      {
          vec2 tc = v_texcoord;

          // Apply inverse homography if corner warp is active
          if(warpEnabled > 0.5)
          {
              // Homography is computed in Y-down space (matching GUI).
              // On OpenGL v_texcoord.y is flipped, so convert to Y-down first.
#if !defined(QSHADER_SPIRV)
              tc.y = 1.0 - tc.y;
#endif
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
#if !defined(QSHADER_SPIRV)
              tc.y = 1.0 - tc.y;
#endif
          }

          vec2 uv;
          uv.x = sourceRect.x + tc.x * sourceRect.z;
#if defined(QSHADER_SPIRV)
          uv.y = sourceRect.y + (1.0 - tc.y) * sourceRect.w;
#else
          float texSrcY = 1.0 - sourceRect.y - sourceRect.w;
          uv.y = texSrcY + tc.y * sourceRect.w;
#endif
          fragColor = texture(tex, uv);

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

    // Create per-window pipeline data
    const auto& outputs = m_multiNode.windowOutputs();
    m_perWindow.resize(outputs.size());

    for(int i = 0; i < (int)outputs.size(); ++i)
    {
      auto& pw = m_perWindow[i];
      const auto& wo = outputs[i];
      pw.sourceRect = wo.sourceRect;

      // Create UBO for source rect
      pw.uvRectUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 4 * sizeof(float));
      pw.uvRectUBO->setName(QByteArray("MultiWindowRenderer::uvRectUBO_") + QByteArray::number(i));
      pw.uvRectUBO->create();

      // Upload initial source rect values
      float rectData[4] = {
          (float)pw.sourceRect.x(), (float)pw.sourceRect.y(),
          (float)pw.sourceRect.width(), (float)pw.sourceRect.height()};
      res.updateDynamicBuffer(pw.uvRectUBO, 0, sizeof(rectData), rectData);

      // Create UBO for blend parameters (4 widths + 4 gammas = 8 floats)
      pw.blendUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 8 * sizeof(float));
      pw.blendUBO->setName(QByteArray("MultiWindowRenderer::blendUBO_") + QByteArray::number(i));
      pw.blendUBO->create();

      float blendData[8] = {
          wo.blendLeft.width, wo.blendRight.width, wo.blendTop.width, wo.blendBottom.width,
          wo.blendLeft.gamma, wo.blendRight.gamma, wo.blendTop.gamma, wo.blendBottom.gamma};
      res.updateDynamicBuffer(pw.blendUBO, 0, sizeof(blendData), blendData);

      // Create UBO for corner warp (3 vec4 columns + 1 float enabled = 52 bytes, round to 64)
      pw.warpUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16 * sizeof(float));
      pw.warpUBO->setName(QByteArray("MultiWindowRenderer::warpUBO_") + QByteArray::number(i));
      pw.warpUBO->create();

      {
        float warpEnabled = wo.cornerWarp.isIdentity() ? 0.0f : 1.0f;
        auto hom = computeHomographyUBO(wo.cornerWarp);
        // 12 floats for 3 vec4 columns, then 1 float for warpEnabled (padded to vec4)
        float warpData[16] = {};
        std::copy(hom.begin(), hom.end(), warpData);
        warpData[12] = warpEnabled;
        res.updateDynamicBuffer(pw.warpUBO, 0, sizeof(warpData), warpData);
      }

      // Create sampler pointing to input texture
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->setName("MultiWindowRenderer::sampler");
      sampler->create();

      pw.samplers[0] = {sampler, m_inputTarget.texture};

      // Build pipeline for this window's render pass descriptor
      // The actual render target is fetched at render time from the swap chain
      if(wo.renderPassDescriptor)
      {
        score::gfx::TextureRenderTarget rt;
        rt.renderPass = wo.renderPassDescriptor;

        QRhiShaderResourceBinding extraBindings[2] = {
          QRhiShaderResourceBinding::uniformBuffer(
              4, QRhiShaderResourceBinding::FragmentStage, pw.blendUBO),
          QRhiShaderResourceBinding::uniformBuffer(
              5, QRhiShaderResourceBinding::FragmentStage, pw.warpUBO)
        };

        pw.pipeline = score::gfx::buildPipeline(
            renderer, mesh, m_vertexS, m_fragmentS, rt, nullptr, pw.uvRectUBO,
            pw.samplers, {extraBindings, 2});
      }
    }
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    for(auto& pw : m_perWindow)
    {
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
    m_perWindow.clear();
    m_inputTarget.release();
  }

  // Called during RenderList::render() for the primary window (index 0)
  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    if(m_perWindow.empty())
      return;

    renderSubRegion(0, renderer, cb, res);
  }

  // Called by MultiWindowNode::render() for secondary windows (index >= 1)
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
  for(auto& wo : m_windowOutputs)
  {
    if(wo.swapChain)
    {
      wo.swapChain->deleteLater();
      wo.swapChain = nullptr;
    }
    delete wo.depthStencil;
    wo.depthStencil = nullptr;
    delete wo.renderPassDescriptor;
    wo.renderPassDescriptor = nullptr;
  }

  if(m_renderState)
  {
    // Window 0's RPD was already deleted in the loop above
    m_renderState->renderPassDescriptor = nullptr;
    delete m_renderState->rhi;
    m_renderState->rhi = nullptr;
    delete m_renderState->surface;
    m_renderState->surface = nullptr;
  }
}

bool MultiWindowNode::canRender() const
{
  return !m_windowOutputs.empty() && m_renderState && m_renderState->rhi;
}

void MultiWindowNode::setRenderSize(QSize sz)
{
  if(m_renderState)
  {
    if(sz.width() >= 1 && sz.height() >= 1)
      m_renderState->renderSize = sz;
  }
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
  auto rl = m_renderer.lock();
  if(!rl || rl->renderers.size() <= 1)
  {
    // No active render graph — clear all windows to black
    renderBlack();
    return;
  }

  auto rhi = m_renderState->rhi;

  // Handle swap chain resizes for all windows
  for(auto& wo : m_windowOutputs)
  {
    if(!wo.window || !wo.swapChain)
      continue;
    if(wo.swapChain->currentPixelSize() != wo.swapChain->surfacePixelSize())
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
    }
  }

  // Phase 1: Render upstream graph + blit sub-region 0 via the primary window
  if(m_windowOutputs.empty())
    return;

  auto& primaryWO = m_windowOutputs[0];
  if(!primaryWO.window || !primaryWO.swapChain || !primaryWO.hasSwapChain)
    return;

  QRhi::FrameOpResult r = rhi->beginFrame(primaryWO.swapChain);
  if(r == QRhi::FrameOpSwapChainOutOfDate)
  {
    primaryWO.hasSwapChain = primaryWO.swapChain->createOrResize();
    if(!primaryWO.hasSwapChain)
      return;
    r = rhi->beginFrame(primaryWO.swapChain);
  }
  if(r != QRhi::FrameOpSuccess)
    return;

  auto cb = primaryWO.swapChain->currentFrameCommandBuffer();
  rl->render(*cb);

  rhi->endFrame(primaryWO.swapChain);

  // Phase 2: For each additional window, blit the sub-region
  if(this->renderedNodes.empty())
    return;
  auto outRenderer = dynamic_cast<MultiWindowRenderer*>(
      this->renderedNodes.begin()->second);
  if(!outRenderer)
    return;

  for(int i = 1; i < (int)m_windowOutputs.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    if(!wo.window || !wo.swapChain || !wo.hasSwapChain)
      continue;

    r = rhi->beginFrame(wo.swapChain);
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(!wo.hasSwapChain)
        continue;
      r = rhi->beginFrame(wo.swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
      continue;

    cb = wo.swapChain->currentFrameCommandBuffer();
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

void MultiWindowNode::initWindow(int index, GraphicsApi api)
{
  auto& wo = m_windowOutputs[index];
  auto& mapping = m_mappings[index];

  auto rhi = m_renderState->rhi;

  // Create swap chain
  wo.swapChain = rhi->newSwapChain();
  wo.swapChain->setWindow(wo.window.get());

  wo.depthStencil = rhi->newRenderBuffer(
      QRhiRenderBuffer::DepthStencil, QSize(),
      m_renderState->samples, QRhiRenderBuffer::UsedWithSwapChainOnly);
  wo.swapChain->setDepthStencil(wo.depthStencil);
  wo.swapChain->setSampleCount(m_renderState->samples);

  QRhiSwapChain::Flags flags = QRhiSwapChain::MinimalBufferCount;
  // Only first window may use VSync; others use NoVSync
  if(index > 0)
    flags |= QRhiSwapChain::NoVSync;
  else if(!score::AppContext().settings<Gfx::Settings::Model>().getVSync())
    flags |= QRhiSwapChain::NoVSync;
  wo.swapChain->setFlags(flags);

  wo.renderPassDescriptor = wo.swapChain->newCompatibleRenderPassDescriptor();
  wo.swapChain->setRenderPassDescriptor(wo.renderPassDescriptor);

  // Store the first window's RPD in the render state
  if(index == 0)
    m_renderState->renderPassDescriptor = wo.renderPassDescriptor;

  wo.sourceRect = mapping.sourceRect;

  wo.blendLeft = {mapping.blendLeft.width, mapping.blendLeft.gamma};
  wo.blendRight = {mapping.blendRight.width, mapping.blendRight.gamma};
  wo.blendTop = {mapping.blendTop.width, mapping.blendTop.gamma};
  wo.blendBottom = {mapping.blendBottom.width, mapping.blendBottom.gamma};
  wo.cornerWarp = mapping.cornerWarp;
}

void MultiWindowNode::createOutput(score::gfx::OutputConfiguration conf)
{
  if(m_mappings.empty())
    return;

  // Create shared QRhi without a specific window
  m_renderState = score::gfx::createRenderState(conf.graphicsApi, QSize{1280, 720}, nullptr);
  if(!m_renderState || !m_renderState->rhi)
    return;

  m_windowOutputs.resize(m_mappings.size());

  // Create all windows
  for(int i = 0; i < (int)m_mappings.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    auto& mapping = m_mappings[i];

    wo.window = std::make_shared<Window>(conf.graphicsApi);
    wo.window->setTitle(
        QString("Output %1").arg(i));

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
      // On Windows, showFullScreen() can default to the primary monitor
      // unless the window is already positioned within the target screen.
      // Set geometry to the target screen's bounds first to anchor it.
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
      wo.window->setGeometry(
          QRect(mapping.windowPosition, mapping.windowSize));
      wo.window->show();
    }
  }

  // Notify the device that windows are available for signal connections
  if(onWindowsCreated)
    onWindowsCreated();

  // Set up a callback to initialize swap chains once first window is exposed
  auto initAll = [this, onReady = std::move(conf.onReady)]() mutable {
    for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
    {
      initWindow(i, m_renderState->api);
      auto& wo = m_windowOutputs[i];
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(wo.hasSwapChain && wo.window)
      {
        m_renderState->outputSize = wo.swapChain->currentPixelSize();
      }
    }

    if(onReady)
      onReady();
  };

  // The first window's onWindowReady triggers initialization
  m_windowOutputs[0].window->onWindowReady = std::move(initAll);

  // Wire resize for all windows
  for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
  {
    m_windowOutputs[i].window->onResize = [this, i, onResize = conf.onResize] {
      if(i < (int)m_windowOutputs.size())
      {
        auto& wo = m_windowOutputs[i];
        if(wo.swapChain)
          wo.hasSwapChain = wo.swapChain->createOrResize();
      }
      // Only trigger pipeline rebuild from first window resize
      if(i == 0 && onResize)
        onResize();
    };
  }
}

void MultiWindowNode::destroyOutput()
{
  for(auto& wo : m_windowOutputs)
  {
    delete wo.depthStencil;
    wo.depthStencil = nullptr;

    // Don't delete RPD for index 0 as it's owned by m_renderState
    // Actually, we manage them all individually
    if(wo.renderPassDescriptor && wo.renderPassDescriptor != m_renderState->renderPassDescriptor)
    {
      delete wo.renderPassDescriptor;
    }
    wo.renderPassDescriptor = nullptr;

    delete wo.swapChain;
    wo.swapChain = nullptr;

    wo.window.reset();
  }
  m_windowOutputs.clear();

  if(m_renderState)
  {
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    m_renderState->destroy();
  }
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
