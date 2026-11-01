#include "PreviewHubNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Settings/Model.hpp>

#include <QtGui/private/qrhi_p.h>

#include <algorithm>

namespace score::gfx
{
namespace
{
//! Gives each input of the hub a target of the size its preview asks for.
//! They are built with the list and rebuilt with the output's targets
//! (PreviewHubNode::inputsChanged): a pass records into the target it was
//! built against, so a lookup must never replace one.
class PreviewHubRenderer final : public OutputNodeRenderer
{
public:
  PreviewHubRenderer(const PreviewHubNode& n, RenderList&)
      : OutputNodeRenderer{n}
      , m_hub{n}
      , m_targets(n.input.size())
  {
  }

  TextureRenderTarget renderTargetForInput(const Port& p) override
  {
    const auto it = std::find(m_hub.input.begin(), m_hub.input.end(), &p);
    if(it == m_hub.input.end())
      return {};
    return m_targets[it - m_hub.input.begin()];
  }

  void init(RenderList& r, QRhiResourceUpdateBatch&) override
  {
    for(std::size_t k = 0; k < m_targets.size(); k++)
    {
      m_targets[k].release();
      const QSize sz = m_hub.inputSize(int(k));
      // A free input: nothing renders into it
      if(sz.isEmpty())
        continue;
      // A thumbnail: single-sampled, with depth only for a producer drawing
      // with it
      m_targets[k] = createRenderTarget(
          r.state, QRhiTexture::RGBA8, sz, 1, r.requiresDepth(*m_hub.input[k]));
    }
  }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void release(RenderList&) override
  {
    for(auto& rt : m_targets)
      rt.release();
  }

private:
  const PreviewHubNode& m_hub;
  std::vector<TextureRenderTarget> m_targets;
};
}

PreviewHubNode::PreviewHubNode()
    : m_sizes(max_inputs)
{
  for(int i = 0; i < max_inputs; i++)
    input.push_back(new Port{this, {}, Types::Image, {}});
  m_conf = {.manualRenderingRate = 1000. / Gfx::Settings::renderRateForCurrentApplication()};
  previewHub = true;
}

PreviewHubNode::~PreviewHubNode()
{
  destroyOutput();
}

int PreviewHubNode::acquireInput()
{
  for(int k = 0; k < max_inputs; k++)
    if(m_sizes[k].isEmpty())
    {
      // Taken from now on; the preview gives the size it wants next
      m_sizes[k] = QSize{16, 16};
      inputsChanged();
      return k;
    }
  return -1;
}

void PreviewHubNode::releaseInput(int k)
{
  if(k >= 0 && k < max_inputs && !m_sizes[k].isEmpty())
  {
    m_sizes[k] = {};
    inputsChanged();
  }
}

bool PreviewHubNode::anyInputUsed() const noexcept
{
  return std::any_of(m_sizes.begin(), m_sizes.end(), [](QSize s) { return !s.isEmpty(); });
}

void PreviewHubNode::setInputSize(int k, QSize size)
{
  if(k < 0 || k >= max_inputs || size.isEmpty() || m_sizes[k] == size)
    return;
  m_sizes[k] = size;
  inputsChanged();
}

void PreviewHubNode::inputsChanged()
{
  if(!m_renderState)
    return;
  const QSize sz = renderSize();
  if(sz != m_renderState->renderSize)
  {
    m_renderState->renderSize = sz;
    m_renderState->outputSize = sz;
    // What follows the render size is rebuilt
    if(m_onResize)
      m_onResize();
  }
  // The targets of the inputs, and the passes drawing into them, are rebuilt
  // even when the largest input keeps its size
  if(auto r = m_renderer.lock())
    r->markOutputTargetsStale();
}

QSize PreviewHubNode::inputSize(int k) const noexcept
{
  return k >= 0 && k < max_inputs ? m_sizes[k] : QSize{};
}

QSize PreviewHubNode::renderSize() const noexcept
{
  QSize sz{16, 16};
  for(auto s : m_sizes)
    sz = sz.expandedTo(s);
  return sz;
}

void PreviewHubNode::render()
{
  if(m_deviceLost)
    return;
  auto renderer = m_renderer.lock();
  if(!renderer || !m_renderState || renderer->renderers.size() <= 1)
    return;
  auto& rhi = *m_renderState->rhi;
  OffscreenFrame frame{rhi};
  if(!frame)
  {
    checkDeviceLost(frame, rhi, "PreviewHubNode");
    return;
  }
  renderer->render(frame.commands());
  frame.end();
  checkDeviceLost(frame, rhi, "PreviewHubNode");
}

void PreviewHubNode::createOutput(OutputConfiguration conf)
{
  m_onResize = conf.onResize;
  m_onReleaseRenderList = conf.onReleaseRenderList;
  resetDeviceLost();
  // The device the previews share
  m_renderState
      = createRenderState(conf.graphicsApi, renderSize(), nullptr, SharedDeviceMode::Cached);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "PreviewHubNode: failed to create QRhi";
    m_renderState.reset();
    return;
  }
  m_renderState->outputSize = m_renderState->renderSize;
  m_renderState->renderFormat = QRhiTexture::RGBA8;
  m_renderState->samples = 1;
  createTarget();
  conf.onReady();
}

void PreviewHubNode::createTarget()
{
  auto& rhi = *m_renderState->rhi;
  m_texture = rhi.newTexture(QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::RenderTarget);
  m_texture->create();
  m_renderTarget = rhi.newTextureRenderTarget({QRhiColorAttachment(m_texture)});
  m_renderState->renderPassDescriptor = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->create();
}

void PreviewHubNode::releaseTarget()
{
  delete m_renderTarget;
  m_renderTarget = nullptr;
  delete m_texture;
  m_texture = nullptr;
  if(m_renderState)
  {
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
  }
}

void PreviewHubNode::destroyOutput()
{
  if(!m_renderState)
    return;
  if(m_renderState->rhi)
    m_renderState->rhi->finish();
  releaseRegistry();
  releaseTarget();
  m_renderState->destroy();
  m_renderState.reset();
}

void PreviewHubNode::updateGraphicsAPI(GraphicsApi api)
{
  if(m_renderState && m_renderState->api != api)
    destroyOutput();
}

TextureRenderTarget PreviewHubNode::currentRenderTarget() const noexcept
{
  if(!m_renderState)
    return {};
  return {
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};
}

OutputNodeRenderer* PreviewHubNode::createRenderer(RenderList& r) const noexcept
{
  return new PreviewHubRenderer{*this, r};
}
}
