#include "SpoutOutput.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>

#include <QtGui/private/qrhigles2_p_p.h>

#include <QFormLayout>
#include <QLabel>

#include <Spout/SpoutSender.h>
#include <score/gfx/OpenGL.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::SpoutDevice)

namespace Gfx
{
struct SpoutNode final : score::gfx::OutputNode
{
  SpoutNode(const SharedOutputSettings& set)
    : OutputNode{}
    , m_settings{set}
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  virtual ~SpoutNode() { }

  void startRendering() override
  {
    if(!m_created)
      m_created = m_spout->CreateSender(m_settings.path.toStdString().c_str(), m_settings.width, m_settings.height);
  }

  void onRendererChange() override { }
  bool canRender() const override
  {
    return bool(m_spout);
  }

  void render() override
  {
    if (m_update)
      m_update();

    auto renderer = m_renderer.lock();
    if (renderer && m_renderState)
    {
      auto rhi = m_renderState->rhi;
      QRhiCommandBuffer* cb{};
      if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        return;

      renderer->render(*cb);

      // Spout-specific part starts here:
      {
        rhi->makeThreadLocalNativeContextCurrent();

        if (m_created)
        {
          auto tex = dynamic_cast<QGles2Texture*>(m_texture)->texture;
          m_spout->SendTexture(tex, GL_TEXTURE_2D, m_settings.width, m_settings.height);
        }
        else
        {
          qDebug() << "sender not created";
        }
      }

      rhi->endOffscreenFrame();
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
#include <Gfx/Qt5CompatPush> // clang-format: keep
    m_spout = std::make_shared<SpoutSender>();
    m_renderState = std::make_shared<score::gfx::RenderState>();
    m_update = onUpdate;

    m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
    QRhiGles2InitParams params;
    params.fallbackSurface = m_renderState->surface;
    score::GLCapabilities caps;
    caps.setupFormat(params.format);
#include <Gfx/Qt5CompatPop> // clang-format: keep
    m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
#include <Gfx/Qt5CompatPush> // clang-format: keep
    m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
    m_renderState->api = score::gfx::GraphicsApi::OpenGL;
    m_renderState->version = caps.qShaderVersion;

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

    onReady();
  }
  void destroyOutput() override
  {
    if (m_spout)
      m_spout->ReleaseSender();
    m_spout.reset();
  }

  score::gfx::RenderState* renderState() const override
  {
    return m_renderState.get();
  }

  score::gfx::OutputNodeRenderer* createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    class SpoutRenderer : public score::gfx::OutputNodeRenderer
    {
    public:
      SpoutRenderer(const score::gfx::RenderState& state, const SpoutNode& parent)
        : score::gfx::OutputNodeRenderer{}
      {
        m_rt.renderTarget = parent.m_renderTarget;
        m_rt.renderPass = state.renderPassDescriptor;
      }

      score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override { return m_rt; }
      void finishFrame(score::gfx::RenderList& renderer, QRhiCommandBuffer& cb) override { }
      void init(score::gfx::RenderList& renderer) override { }
      void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
      void release(score::gfx::RenderList&) override { }
    private:
      score::gfx::TextureRenderTarget m_rt;
    };
    return new SpoutRenderer{r.state, *this};
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
  std::shared_ptr<SpoutSender> m_spout{};
  bool m_created{};
};

#include <Gfx/Qt5CompatPop> // clang-format: keep

SpoutDevice::~SpoutDevice() { }

bool SpoutDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      auto set = m_settings.deviceSpecificSettings.value<SharedOutputSettings>();
      m_protocol = new gfx_protocol_base{plug->exec};

      class spout_device : public ossia::net::device_base
      {
        gfx_node_base root;

      public:
        spout_device(
            const SharedOutputSettings& set,
            std::unique_ptr<ossia::net::protocol_base> proto,
            std::string name)
            : ossia::net::device_base{std::move(proto)}
            , root{*this, new SpoutNode{set}, name}
        {
        }

        const gfx_node_base& get_root_node() const override { return root; }
        gfx_node_base& get_root_node() override { return root; }
      };

      m_dev = std::make_unique<spout_device>(
          set,
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          m_settings.name.toStdString());
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

QString SpoutProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Spout Output");
}

Device::DeviceInterface* SpoutProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& doc,
    const score::DocumentContext& ctx)
{
  return new SpoutDevice(settings, ctx);
}

const Device::DeviceSettings&
SpoutProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = static_concreteKey();
    s.name = "Spout Out";
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

Device::ProtocolSettingsWidget* SpoutProtocolFactory::makeSettingsWidget()
{
  return new SpoutSettingsWidget;
}

SpoutSettingsWidget::SpoutSettingsWidget(QWidget* parent)
    : SharedOutputSettingsWidget(parent)
{
  m_deviceNameEdit->setText("Spout Out");

  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Identifier");
  setSettings(SpoutProtocolFactory{}.defaultSettings());
}

Device::DeviceSettings SpoutSettingsWidget::getSettings() const
{
  auto set = SharedOutputSettingsWidget::getSettings();
  set.protocol = SpoutProtocolFactory::static_concreteKey();
  return set;
}
}
