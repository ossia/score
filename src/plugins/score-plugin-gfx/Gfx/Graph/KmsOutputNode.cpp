#include "KmsOutputNode.hpp"

#if defined(SCORE_HAS_DRM_KMS) && defined(__linux__)

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <score/gfx/QRhiGles2.hpp>
#include <Gfx/Graph/interop/DrmKmsDevice.hpp>
#include <Gfx/Graph/interop/EglDmaBufExport.hpp>
#include <Gfx/Graph/interop/EglDmaBufImport.hpp>

#include <QDebug>

#include <algorithm>
#include <deque>
#include <vector>

namespace score::gfx
{
namespace
{
// 'XR24'. Spelled out so this builds without <drm/drm_fourcc.h>, the
// same convention DrmFourcc.hpp uses.
constexpr std::uint32_t kFourccXRGB8888 = 0x34325258u;
constexpr std::uint64_t kModifierLinear = 0ull;
constexpr std::uint64_t kModifierInvalid = 0x00ffffffffffffffull;
}

struct KmsOutputNode::Impl
{
  KmsOutputSettings set;

  drm::KmsDevice kms;
  GbmDmaBufExport gbm;
  EglDmaBufImporter egl;

  QRhi* rhi{};
  std::shared_ptr<RenderState> state;
  std::weak_ptr<RenderList> renderer;

  struct Slot
  {
    GbmDmaBufExport::Slot buf;
    QRhiTexture* tex{};
    QRhiTextureRenderTarget* rt{};
    std::uint32_t fbId{};
  };
  std::vector<Slot> slots;
  std::size_t current{0};

  /// Slots flipped to whose completion event has not been read yet, oldest
  /// first. The kernel delivers page-flip events in the order they were queued,
  /// so draining from the front is exactly in step with the display.
  std::deque<std::size_t> pending;

  QRhiRenderPassDescriptor* rpDesc{};
  TextureRenderTarget currentTarget;

  drm::ConnectorInfo connector;
  drm::ModeInfo mode;
  std::uint64_t negotiatedModifier{kModifierInvalid};
  bool running{false};
  bool modesetDone{false};
  std::string description;

  std::uint64_t flips{0}, waited{0};

  /// Modifiers the target plane advertises for `fourcc`, best-first.
  ///
  /// This is what keeps the path honest: allocating something the plane cannot
  /// scan out either fails at addFramebuffer or gets silently detiled somewhere,
  /// and a detiling copy is exactly what this output exists to avoid.
  std::vector<std::uint64_t> planeModifiers(std::uint32_t fourcc) const
  {
    std::vector<std::uint64_t> out;
    for(const auto& p : kms.info().planes)
    {
      for(const auto& f : p.formats)
      {
        if(f.fourcc != fourcc)
          continue;
        for(auto m : f.modifiers)
          if(m != kModifierInvalid
             && std::find(out.begin(), out.end(), m) == out.end())
            out.push_back(m);
      }
    }
    return out;
  }

  void releaseSlots()
  {
    for(auto& s : slots)
    {
      if(s.fbId)
        kms.removeFramebuffer(s.fbId);
      delete s.rt;
      delete s.tex;
      s.rt = nullptr;
      s.tex = nullptr;
      s.fbId = 0;
    }
    delete rpDesc;
    rpDesc = nullptr;
    slots.clear();
    current = 0;
  }
};

KmsOutputNode::KmsOutputNode(KmsOutputSettings settings)
    : d{std::make_unique<Impl>()}
{
  d->set = std::move(settings);
  // The image input the graph renders into. A sink without it is a node nothing
  // can be connected to, and the first thing that touches input[0] walks off the
  // end of an empty vector.
  input.push_back(new Port{this, {}, Types::Image, {}});
}

KmsOutputNode::~KmsOutputNode()
{
  destroyOutput();
}

void KmsOutputNode::setRenderer(std::shared_ptr<RenderList> r)
{
  d->renderer = r;
}
RenderList* KmsOutputNode::renderer() const
{
  auto r = d->renderer.lock();
  return r.get();
}
void KmsOutputNode::onRendererChange() { }

bool KmsOutputNode::canRender() const
{
  return d->running && !d->slots.empty() && d->rhi;
}

void KmsOutputNode::startRendering()
{
  d->running = true;
}
void KmsOutputNode::stopRendering()
{
  d->running = false;
}

std::shared_ptr<RenderState> KmsOutputNode::renderState() const
{
  return d->state;
}

OutputNode::Configuration KmsOutputNode::configuration() const noexcept
{
  Configuration c;
  // Paced by the flip itself: render() blocks on the previous flip's completion,
  // so the display's own vblank is the clock. A timer on top would either race
  // it or add a frame of latency.
  c.manualRenderingRate = d->mode.refreshMilliHz > 0
                              ? 1000.0 / (d->mode.refreshMilliHz / 1000.0)
                              : 16.67;
  c.outputNeedsRenderPass = true;
  c.supportsVSync = true;
  return c;
}

TextureRenderTarget KmsOutputNode::currentRenderTarget() const noexcept
{
  return d->currentTarget;
}

std::string KmsOutputNode::engagedDescription() const
{
  return d->description;
}

namespace
{
/// Hands the graph the slot being rendered into this frame. The target rotates,
/// which is what OutputNode::currentRenderTarget() exists for; all slots share
/// one render-pass descriptor so pipelines stay valid across the rotation.
class KmsRenderer final : public OutputNodeRenderer
{
public:
  KmsRenderer(const score::gfx::Node& n, KmsOutputNode::Impl& impl)
      : OutputNodeRenderer{n}
      , m_impl{impl}
  {
  }

  TextureRenderTarget renderTargetForInput(const Port&) override
  {
    return m_impl.currentTarget;
  }
  void init(RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void runRenderPass(RenderList&, QRhiCommandBuffer&, Edge&) override { }
  void release(RenderList&) override { }

private:
  KmsOutputNode::Impl& m_impl;
};
}

OutputNodeRenderer* KmsOutputNode::createRenderer(RenderList& r) const noexcept
{
  return new KmsRenderer{*this, *d};
}

void KmsOutputNode::createOutput(OutputConfiguration conf)
{
  // GL only for now: the scanout buffers reach QRhi as GL textures wrapped with
  // createFrom, and the Vulkan equivalent (importing the dma-buf as a VkImage and
  // rendering into it) is a separate path rather than a variation of this one.
  if(conf.graphicsApi != GraphicsApi::OpenGL)
  {
    qWarning() << "KMS output: only the OpenGL backend is supported so far";
    return;
  }

  // --- device + connector ---------------------------------------------------
  bool opened = false;
  if(!d->set.device.empty())
  {
    opened = d->kms.open(d->set.device);
  }
  else
  {
    for(const auto& dev : drm::KmsDevice::enumerateDevices())
    {
      const bool anyConnected = std::any_of(
          dev.connectors.begin(), dev.connectors.end(),
          [](const drm::ConnectorInfo& c) { return c.connected && !c.writeback; });
      if(anyConnected && d->kms.open(dev.path))
      {
        opened = true;
        break;
      }
    }
  }
  if(!opened)
  {
    qWarning() << "KMS output: no usable card node;" << d->kms.lastError().c_str();
    return;
  }

  if(!d->kms.acquireMaster())
  {
    // Expected on a desktop: a compositor holds master. Degrade rather than
    // abort, and say why -- "no output" with no explanation is the worst case.
    qWarning() << "KMS output: cannot take DRM master (a compositor most likely "
                  "holds it). A DRM lease would be needed to share the device; "
                  "KmsDevice does not implement leases yet.";
    d->kms.close();
    return;
  }

  const auto& info = d->kms.info();
  const drm::ConnectorInfo* conn = nullptr;
  for(const auto& c : info.connectors)
  {
    if(c.writeback || !c.connected || c.modes.empty())
      continue;
    if(d->set.connectorId == 0 || c.id == d->set.connectorId)
    {
      conn = &c;
      break;
    }
  }
  if(!conn)
  {
    qWarning() << "KMS output: no connected connector";
    destroyOutput();
    return;
  }
  d->connector = *conn;

  d->mode = conn->modes.front();
  for(const auto& m : conn->modes)
  {
    if(d->set.width && d->set.height)
    {
      if(m.width == d->set.width && m.height == d->set.height)
      {
        d->mode = m;
        if(d->set.refreshRate <= 0.0
           || std::abs(m.refreshMilliHz / 1000.0 - d->set.refreshRate) < 0.5)
          break;
      }
    }
    else if(m.preferred)
    {
      d->mode = m;
      break;
    }
  }

  // --- scanout buffers ------------------------------------------------------
  if(!d->gbm.init())
  {
    qWarning() << "KMS output: libgbm unavailable";
    destroyOutput();
    return;
  }
  const QSize size{int(d->mode.width), int(d->mode.height)};
  d->state = score::gfx::createRenderState(conf.graphicsApi, size, nullptr);
  if(!d->state || !d->state->rhi)
  {
    qWarning() << "KMS output: failed to create the render state";
    destroyOutput();
    return;
  }
  d->state->renderSize = size;
  d->state->outputSize = size;
  d->state->api = conf.graphicsApi;
  d->state->renderFormat = QRhiTexture::RGBA8;
  d->rhi = d->state->rhi;
  if(!d->egl.init(*d->rhi))
  {
    qWarning() << "KMS output: EGL dma-buf import unavailable";
    destroyOutput();
    return;
  }

  const std::uint32_t fourcc = kFourccXRGB8888;
  auto mods = d->planeModifiers(fourcc);
  if(mods.empty())
    mods.push_back(kModifierLinear);

  const auto n = std::max<std::size_t>(d->set.bufferCount, 2);
  d->slots.resize(n);
  for(std::size_t i = 0; i < n; ++i)
  {
    auto& s = d->slots[i];
    // GBM_BO_USE_SCANOUT is what makes the allocation acceptable to the display
    // engine; without it the buffer may be placed where the CRTC cannot read it.
    constexpr std::uint32_t kScanout
        = GbmDmaBufExport::GBM_BO_USE_SCANOUT_v;
    if(!d->gbm.allocSlot(
           s.buf, d->mode.width, d->mode.height, fourcc, d->egl, kScanout))
    {
      qWarning() << "KMS output: scanout buffer" << int(i) << "allocation failed";
      destroyOutput();
      return;
    }
    if(i == 0)
      d->negotiatedModifier = s.buf.modifier;

    drm::FramebufferDesc fb;
    fb.width = s.buf.width;
    fb.height = s.buf.height;
    fb.fourcc = fourcc;
    fb.modifier = s.buf.modifier;
    fb.fd[0] = s.buf.fd;
    fb.stride[0] = s.buf.stride;
    fb.offset[0] = s.buf.offset;
    fb.planeCount = 1;
    s.fbId = d->kms.addFramebuffer(fb);
    if(!s.fbId)
    {
      qWarning() << "KMS output: the kernel refused a framebuffer for modifier"
                 << drm::modifierName(s.buf.modifier).c_str()
                 << "-- the allocation does not match what the plane advertises;"
                 << d->kms.lastError().c_str();
      destroyOutput();
      return;
    }

    s.tex = d->rhi->newTexture(
        QRhiTexture::RGBA8, QSize(int(s.buf.width), int(s.buf.height)), 1,
        QRhiTexture::RenderTarget);
    if(!s.tex
       || !s.tex->createFrom(
           QRhiTexture::NativeTexture{quint64(s.buf.glTexture), 0}))
    {
      qWarning() << "KMS output: cannot wrap scanout buffer" << int(i)
                 << "as a QRhi texture";
      destroyOutput();
      return;
    }

    s.rt = d->rhi->newTextureRenderTarget({s.tex});
    // One descriptor shared by every slot: they are format- and
    // sample-compatible by construction, and pipelines are built against the
    // descriptor rather than the target, so rotating targets must not rotate it.
    if(i == 0)
    {
      d->rpDesc = s.rt->newCompatibleRenderPassDescriptor();
      d->state->renderPassDescriptor = d->rpDesc;
    }
    s.rt->setRenderPassDescriptor(d->rpDesc);
    if(!s.rt->create())
    {
      qWarning() << "KMS output: render target" << int(i) << "creation failed";
      destroyOutput();
      return;
    }
  }

  d->current = 0;
  d->currentTarget = TextureRenderTarget{
      d->slots[0].tex, nullptr, nullptr, d->rpDesc, d->slots[0].rt};

  if(!d->kms.atomicModeset(d->connector.id, d->mode, d->slots[0].fbId))
  {
    qWarning() << "KMS output: modeset failed;" << d->kms.lastError().c_str();
    destroyOutput();
    return;
  }
  d->modesetDone = true;

  d->description = d->kms.info().path + " " + d->connector.name + " "
                   + d->mode.name + " @ "
                   + std::to_string(d->mode.refreshMilliHz / 1000.0) + " Hz, "
                   + drm::fourccName(fourcc) + " "
                   + drm::modifierName(d->negotiatedModifier);
  if(d->set.verbose)
  {
    qDebug().noquote() << "KMS output:" << QString::fromStdString(d->description)
                       << "-- rendering straight into scanout, no present blit";
    if(d->negotiatedModifier == kModifierLinear && mods.size() > 1)
      qDebug() << "KMS output: negotiated LINEAR although the plane advertises"
               << int(mods.size()) << "modifiers; scanout works but the GPU may "
                                      "be writing untiled";
  }

  if(conf.onReady)
    conf.onReady();
}

void KmsOutputNode::destroyOutput()
{
  d->running = false;
  d->releaseSlots();
  if(d->modesetDone)
  {
    d->kms.releaseMaster();
    d->modesetDone = false;
  }
  d->kms.close();
  d->currentTarget = {};
  d->state.reset();
  d->rhi = nullptr;
}

void KmsOutputNode::render()
{
  if(!canRender())
    return;
  auto r = d->renderer.lock();
  if(!r)
    return;

  auto& slot = d->slots[d->current];

  // A slot cannot be rendered into while the display may still be scanning it
  // out. Drain completion events until this slot is no longer queued -- events
  // arrive in flip order, so this consumes exactly the ones that came first and
  // never waits for an event that has already been read. With three buffers and
  // vblank pacing the queue is short enough that this rarely blocks; with
  // tearing flips it can, which is why the wait is explicit.
  while(std::find(d->pending.begin(), d->pending.end(), d->current)
        != d->pending.end())
  {
    const auto ev = d->kms.waitFlip(200);
    if(!d->pending.empty())
      d->pending.pop_front();
    if(!ev.valid)
    {
      ++d->waited;
      break;
    }
  }

  d->currentTarget
      = TextureRenderTarget{slot.tex, nullptr, nullptr, d->rpDesc, slot.rt};

  QRhiCommandBuffer* cb{};
  if(d->rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return;
  r->render(*cb);
  d->rhi->endOffscreenFrame();

  if(d->kms.atomicFlip(slot.fbId, d->set.tearing))
  {
    d->pending.push_back(d->current);
    ++d->flips;
    if(d->set.verbose && d->waited && (d->flips % 600) == 0)
      qDebug() << "KMS output:" << d->flips << "flips," << d->waited
               << "stalled waiting for a completion event";
  }
  else if(d->set.verbose && (d->flips % 120) == 0)
  {
    qWarning() << "KMS output: flip rejected;" << d->kms.lastError().c_str();
  }

  d->current = (d->current + 1) % d->slots.size();
}

}

#else

namespace score::gfx
{
struct KmsOutputNode::Impl
{
};
KmsOutputNode::KmsOutputNode(KmsOutputSettings)
    : d{}
{
}
KmsOutputNode::~KmsOutputNode() = default;
void KmsOutputNode::setRenderer(std::shared_ptr<RenderList>) { }
RenderList* KmsOutputNode::renderer() const
{
  return nullptr;
}
OutputNodeRenderer* KmsOutputNode::createRenderer(RenderList&) const noexcept
{
  return nullptr;
}
void KmsOutputNode::startRendering() { }
void KmsOutputNode::render() { }
void KmsOutputNode::stopRendering() { }
bool KmsOutputNode::canRender() const
{
  return false;
}
void KmsOutputNode::onRendererChange() { }
void KmsOutputNode::createOutput(OutputConfiguration) { }
void KmsOutputNode::destroyOutput() { }
std::shared_ptr<RenderState> KmsOutputNode::renderState() const
{
  return {};
}
OutputNode::Configuration KmsOutputNode::configuration() const noexcept
{
  return {};
}
TextureRenderTarget KmsOutputNode::currentRenderTarget() const noexcept
{
  return {};
}
std::string KmsOutputNode::engagedDescription() const
{
  return {};
}
}

#endif
