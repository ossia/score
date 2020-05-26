#include "SpoutDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QFormLayout>
#include <QMenu>
#include <QMimeData>
#include <QtGui/private/qrhigles2_p_p.h>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::SpoutDevice)

#include <QTimer>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>

#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Spout/SpoutSender.h>
namespace Gfx
{
struct SpoutNode : OutputNode
{
  SpoutNode();
  virtual ~SpoutNode();

  Renderer* m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<RenderState> m_renderState{};
  std::shared_ptr<SpoutSender> m_spout{};
  bool m_hasSender{};

  void startRendering() override;
  void onRendererChange() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(Renderer* r) override;
  Renderer* renderer() const override;

  void createOutput(
      GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onResize
      ) override;
  void destroyOutput() override;

  RenderState* renderState() const override;
  RenderedNode* createRenderer() const noexcept override;
};

class spout_device : public ossia::net::device_base
{
  gfx_node_base root;

public:
  spout_device(std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}, root{*this, new SpoutNode, name}
  {
  }

  const gfx_node_base& get_root_node() const override { return root; }
  gfx_node_base& get_root_node() override { return root; }
};
}


namespace Gfx
{

  QTimer* timer_unsafe{};

SpoutNode::SpoutNode()
  : OutputNode{}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
  timer_unsafe = new QTimer;
  QObject::connect(timer_unsafe, &QTimer::timeout,
                   [this] {
    if(m_renderer && m_renderState) {
      auto rhi = m_renderState->rhi;
      QRhiCommandBuffer *cb{};
      if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
          return;

      m_renderer->render(*cb);

      {
        rhi->makeThreadLocalNativeContextCurrent();
        static bool b = m_spout->CreateSender("ossia score", 1280, 720);

        if(b)
        {
          auto tex = dynamic_cast<QGles2Texture*>(m_texture)->texture;
          m_spout->SendTexture(tex, GL_TEXTURE_2D, 1280, 720);
        }
        else
        {
          qDebug() << "sender not created";
        }
      }
      rhi->endOffscreenFrame();
    }
  });
}

SpoutNode::~SpoutNode()
{

}
bool SpoutNode::canRender() const
{
  return bool(m_spout);
}

void SpoutNode::startRendering()
{
  qDebug() << "startRendering: " ;
  timer_unsafe->start(16);
  /*
  if (window)
  {
    window->onRender = [this] {
    if (auto r = window->state.renderer)
    {
      window->canRender = r->renderedNodes.size() > 1;
      r->render();
    }
    };
  }*/
}

void SpoutNode::onRendererChange()
{/*
  if (window)
    if (auto r = window->state.renderer)
      window->canRender = r->renderedNodes.size() > 1;
      */
}

void SpoutNode::stopRendering()
{
  qDebug() << "stopRendering: " ;
  timer_unsafe->stop();
  /*
  if (window)
  {
    window->canRender = false;
    window->onRender = [] {};
    ////window->state.hasSwapChain = false;
  }
  */
}

void SpoutNode::setRenderer(Renderer* r)
{
  m_renderer = r;
}

Renderer* SpoutNode::renderer() const
{
  return m_renderer;
}

void SpoutNode::createOutput(
      GraphicsApi graphicsApi,
      std::function<void ()> onReady,
      std::function<void ()> onResize)
{
  m_spout = std::make_shared<SpoutSender>();
  m_renderState = std::make_shared<RenderState>();

  m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
  QRhiGles2InitParams params;
  params.fallbackSurface = m_renderState->surface;
  m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
  m_renderState->size = QSize(1280, 720);

  auto rhi = m_renderState->rhi;
  m_texture = rhi->newTexture(
                       QRhiTexture::RGBA8,
                       m_renderState->size,
                       1,
                       QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  m_texture->build();
  m_renderTarget = rhi->newTextureRenderTarget({ m_texture });
  m_renderState->renderPassDescriptor = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->build();

  onReady();
}

void SpoutNode::destroyOutput()
{
  if(m_spout)
    m_spout->ReleaseSender();
  m_spout.reset();
}

RenderState* SpoutNode::renderState() const
{
  return m_renderState.get();
}

class SpoutRenderer : public RenderedNode
{
public:
  using RenderedNode::RenderedNode;
  void createRenderTarget(const RenderState& state) override
  {
    auto& self = static_cast<const SpoutNode&>(this->node);
    m_renderTarget = self.m_renderTarget;
    m_renderPass = state.renderPassDescriptor;
  }
};

RenderedNode* SpoutNode::createRenderer() const noexcept
{
  return new SpoutRenderer{*this};
}





SpoutDevice::~SpoutDevice() { }

bool SpoutDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      m_protocol = new gfx_protocol_base{plug->exec};
      m_dev = std::make_unique<spout_device>(
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

QString SpoutProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* SpoutProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* SpoutProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new SpoutDevice(settings, ctx);
}

const Device::DeviceSettings& SpoutProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "spout_out";
    return s;
  }();
  return settings;
}

Device::AddressDialog* SpoutProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* SpoutProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* SpoutProtocolFactory::makeSettingsWidget()
{
  return new SpoutSettingsWidget;
}

QVariant SpoutProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void SpoutProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
}

bool SpoutProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

SpoutSettingsWidget::SpoutSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void SpoutSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("spout_out");
}

Device::DeviceSettings SpoutSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = SpoutProtocolFactory::static_concreteKey();
  return s;
}

void SpoutSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}

}
