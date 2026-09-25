#include "SinkNode.hpp"

#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>

#include <algorithm>

namespace score::gfx
{
SinkNode::SinkNode(double rate)
{
  input.push_back(new Port{this, {}, Types::Image, {}});
  input.push_back(new Port{this, {}, Types::Float, {}});
  setRate(rate);
}

SinkNode::~SinkNode()
{
  destroyOutput();
}

void SinkNode::setRate(double fps) noexcept
{
  m_rate = std::clamp(fps, 0.1, 1000.);
}

void SinkNode::process(Message&& msg)
{
  // Inlet 0 is the texture (its render target spec, if any), 1 the rate.
  if(msg.input.size() > 0)
    if(auto spec = ossia::get_if<ossia::render_target_spec>(&msg.input[0]))
      Node::process(0, *spec);
  if(msg.input.size() > 1)
    if(auto v = ossia::get_if<ossia::value>(&msg.input[1]); v && v->valid())
      setRate(ossia::convert<float>(*v));
}

void SinkNode::startRendering() { }
void SinkNode::stopRendering() { }
void SinkNode::onRendererChange() { }
bool SinkNode::canRender() const
{
  return bool(m_renderState);
}

void SinkNode::render()
{
  const auto now = std::chrono::steady_clock::now();
  const auto period = std::chrono::duration<double>(1. / m_rate);
  // Half a clock tick of slack, so 30 fps on a 60 Hz clock is every other tick
  // and not every third one.
  const auto slack = std::chrono::duration<double>(
      0.5 / std::max(Gfx::Settings::renderRateForCurrentApplication(), 1.));
  if(m_frames > 0 && now - m_last < period - slack)
    return;

  auto renderer = m_renderer.lock();
  if(!renderer || !m_renderState || renderer->renderers.size() <= 1)
    return; // nothing upstream

  OffscreenFrame frame{*m_renderState->rhi};
  if(!frame)
    return;
  renderer->render(frame.commands());
  m_last = now;
  m_frames++;
}

void SinkNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_renderer = r;
}

RenderList* SinkNode::renderer() const
{
  return m_renderer.lock().get();
}

void SinkNode::createOutput(OutputConfiguration conf)
{
  m_onReleaseRenderList = conf.onReleaseRenderList;

  // The upstream nodes render at the output's size unless their own inlets say
  // otherwise: the size set on this process's inlet, else 1280x720.
  QSize size{1280, 720};
  if(auto spec = firstInputRenderTargetSpecs(); spec && spec->size.isValid())
    size = spec->size;

  m_renderState = createRenderState(conf.graphicsApi, size, nullptr);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "SinkNode: failed to create QRhi";
    m_renderState.reset();
    return;
  }
  m_renderState->outputSize = m_renderState->renderSize;

  auto rhi = m_renderState->rhi;
  m_texture = rhi->newTexture(
      QRhiTexture::RGBA8, m_renderState->renderSize, 1, QRhiTexture::RenderTarget);
  m_texture->setName("SinkNode::m_texture");
  m_texture->create();
  m_renderTarget = rhi->newTextureRenderTarget({m_texture});
  m_renderState->renderPassDescriptor
      = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->create();

  if(conf.onReady)
    conf.onReady();
}

void SinkNode::destroyOutput()
{
  if(!m_renderState)
    return;

  if(m_renderState->rhi)
    m_renderState->rhi->finish();

  // The registry outlives the render list: release it while the QRhi lives.
  releaseRegistry();

  delete m_renderTarget;
  m_renderTarget = nullptr;
  delete m_renderState->renderPassDescriptor;
  m_renderState->renderPassDescriptor = nullptr;
  delete m_texture;
  m_texture = nullptr;

  m_renderState->destroy();
  m_renderState.reset();
}

void SinkNode::updateGraphicsAPI(GraphicsApi api)
{
  if(m_renderState && m_renderState->api != api)
    destroyOutput();
}

std::shared_ptr<RenderState> SinkNode::renderState() const
{
  return m_renderState;
}

TextureRenderTarget SinkNode::currentRenderTarget() const noexcept
{
  if(!m_renderState)
    return {};
  return TextureRenderTarget{
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};
}

OutputNodeRenderer* SinkNode::createRenderer(RenderList& r) const noexcept
{
  // No readback: nothing looks at the pixels.
  return new Gfx::BasicRenderer{currentRenderTarget(), *m_renderState, *this};
}

OutputNode::Configuration SinkNode::configuration() const noexcept
{
  // Driven by the host at the application's render rate; render() skips
  // down to m_rate.
  return {.manualRenderingRate = 1000. / Gfx::Settings::renderRateForCurrentApplication()};
}
}
