#include "SyphonOutput.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QUrl>
#include <Gfx/Syphon/SyphonHelpers.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <Syphon/SyphonOpenGLServer.h>
#include <score/gfx/OpenGL.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::SyphonDevice)

namespace Gfx
{
CGLContextObj nativeContext(QRhi& rhi)
{
  auto handles = (QRhiGles2NativeHandles*) rhi.nativeHandles();
  QOpenGLContext* ctx = handles->context;
  auto pc = ctx->nativeInterface<QNativeInterface::QCocoaGLContext>();
  return [pc->nativeContext() CGLContextObj];
}

struct SyphonNode final : score::gfx::OutputNode
{
  SyphonNode(const SharedOutputSettings& set)
    : OutputNode{}
    , m_settings{set}
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  virtual ~SyphonNode() { }

  void startRendering() override
  {
  }

  void onRendererChange() override { }
  bool canRender() const override
  {
    return bool(m_syphon);
  }


  void createSyphon(QRhi& rhi)
  {
    if(!m_created)
    {
      auto serverName = this->m_settings.path.toNSString();
      m_syphon = [[SyphonOpenGLServer alloc]
        initWithName:serverName
        context:nativeContext(rhi)
        options:NULL
      ];
      m_created = true;
    }
  }

  void render() override
  {
    if (m_update)
      m_update();

    auto renderer = m_renderer.lock();
    if (renderer && renderer->nodes.size() > 1 && m_renderState)
    {
      auto rhi = m_renderState->rhi;

      QRhiCommandBuffer* cb{};
      if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        return;

      renderer->render(*cb);

      rhi->endOffscreenFrame();

      // Syphon-specific part starts here:
      {
        rhi->makeThreadLocalNativeContextCurrent();

        createSyphon(*rhi);

        if (m_created)
        {
          auto t = static_cast<QGles2Texture*>(m_texture);

          [m_syphon
            publishFrameTexture:t->texture
            textureTarget:t->target
            imageRegion:NSMakeRect(0, 0, m_settings.width, m_settings.height)
            textureDimensions:NSMakeSize(m_settings.width, m_settings.height)
            flipped:false
          ];
        }
      }
    }
  }

  void stopRendering() override
  {
  }

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }

  score::gfx::RenderList* renderer() const override
  {
    return m_renderer.lock().get();
  }

  void createOutput(
        score::gfx::GraphicsApi graphicsApi,
        std::function<void()> onReady,
        std::function<void()> onUpdate,
        std::function<void()> onResize) override
  {
    m_renderState = std::make_shared<score::gfx::RenderState>();
    m_update = onUpdate;

    m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
    QRhiGles2InitParams params;
    params.format.setMajorVersion(3);
    params.format.setMinorVersion(2);
    params.format.setProfile(QSurfaceFormat::CompatibilityProfile);
    params.fallbackSurface = m_renderState->surface;
    m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
    m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->api = score::gfx::GraphicsApi::OpenGL;
    m_renderState->version = QShaderVersion(120);

    auto rhi = m_renderState->rhi;
    m_texture = rhi->newTexture(
                  QRhiTexture::RGBA8, m_renderState->renderSize, 1,
                  QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    rhi->makeThreadLocalNativeContextCurrent();
    createSyphon(*rhi);
    onReady();
  }

  void destroyOutput() override
  {
    [m_syphon stop];

    m_syphon = nullptr;
  }

  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }

  score::gfx::OutputNodeRenderer* createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    class SyphonRenderer : public score::gfx::OutputNodeRenderer
    {
    public:
      SyphonRenderer(const score::gfx::RenderState& state, const SyphonNode& parent)
        : score::gfx::OutputNodeRenderer{parent}
      {
        m_rt.texture = parent.m_texture;
        m_rt.renderTarget = parent.m_renderTarget;
        m_rt.renderPass = state.renderPassDescriptor;
      }

      score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override { return m_rt; }
      void finishFrame(score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
                       QRhiResourceUpdateBatch*& res) override { }
      void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
      void update(score::gfx::RenderList &renderer,
                  QRhiResourceUpdateBatch &res,
                  score::gfx::Edge *edge) override
      {}
      void release(score::gfx::RenderList&) override { }
    private:
      score::gfx::TextureRenderTarget m_rt;
    };
    return new SyphonRenderer{r.state, *this};
  }

  Configuration configuration() const noexcept override{
    return { .manualRenderingRate = 1000. / m_settings.rate };
  }

private:
  SharedOutputSettings m_settings;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::function<void()> m_update;
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  SyphonOpenGLServer* m_syphon{};
  bool m_created{};
};

SyphonDevice::~SyphonDevice() { }

void SyphonDevice::disconnect()
{
    GfxOutputDevice::disconnect();
    auto prev = std::move(m_dev);
    m_dev = {};
    deviceChanged(prev.get(), nullptr);
}

bool SyphonDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      auto set = m_settings.deviceSpecificSettings.value<SharedOutputSettings>();
      m_protocol = new gfx_protocol_base{plug->exec};

      class syphon_device : public ossia::net::device_base
      {
        gfx_node_base root;

      public:
        syphon_device(
            const SharedOutputSettings& set,
            std::unique_ptr<gfx_protocol_base> proto,
            std::string name)
            : ossia::net::device_base{std::move(proto)}
            , root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), new SyphonNode{set}, name}
        {
        }

        const gfx_node_base& get_root_node() const override { return root; }
        gfx_node_base& get_root_node() override { return root; }
      };

      m_dev = std::make_unique<syphon_device>(
          set,
          std::unique_ptr<gfx_protocol_base>(m_protocol),
          m_settings.name.toStdString());
      deviceChanged(nullptr, m_dev.get());
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

QString SyphonProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Syphon Output");
}

QUrl SyphonProtocolFactory::manual() const noexcept
{
    return QUrl("https://ossia.io/score-docs/devices/syphon-device.html");
}

Device::DeviceInterface* SyphonProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& doc,
    const score::DocumentContext& ctx)
{
  return new SyphonDevice(settings, ctx);
}

const Device::DeviceSettings&
SyphonProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = static_concreteKey();
    s.name = "Syphon Out";
    SharedOutputSettings set;
    set.width = 1280;
    set.height = 720;
    set.path = "ossia score";
    set.rate = 60.;
    s.deviceSpecificSettings = QVariant::fromValue(set);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* SyphonProtocolFactory::makeSettingsWidget()
{
  return new SyphonSettingsWidget;
}

SyphonSettingsWidget::SyphonSettingsWidget(QWidget* parent)
    : SharedOutputSettingsWidget(parent)
{
  m_deviceNameEdit->setText("Syphon Out");

  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Identifier");
  setSettings(SyphonProtocolFactory{}.defaultSettings());
}

Device::DeviceSettings SyphonSettingsWidget::getSettings() const
{
  auto set = SharedOutputSettingsWidget::getSettings();
  set.protocol = SyphonProtocolFactory::static_concreteKey();
  return set;
}
}

