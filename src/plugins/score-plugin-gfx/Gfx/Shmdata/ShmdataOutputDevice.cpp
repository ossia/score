#include "ShmdataOutputDevice.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Qt5CompatPush> // clang-format: keep

#include <score/gfx/OpenGL.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QOffscreenSurface>
#include <QSpinBox>
#include <QtGui/private/qrhigles2_p_p.h>

#include <shmdata/writer.hpp>

#include <wobjectimpl.h>

#include <shmdata/console-logger.hpp>

namespace Gfx
{
class ShmdataOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(ShmdataOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~ShmdataOutputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

class ShmdataOutputSettingsWidget final : public Gfx::SharedOutputSettingsWidget
{
public:
  ShmdataOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
};

}
W_OBJECT_IMPL(Gfx::ShmdataOutputDevice)

namespace Gfx
{
struct ShmdataOutputNode : score::gfx::OutputNode
{
  explicit ShmdataOutputNode(const SharedOutputSettings&);
  virtual ~ShmdataOutputNode();

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::function<void()> m_update;
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::shared_ptr<shmdata::Writer> m_writer{};
  bool m_hasSender{};

  void startRendering() override;
  void onRendererChange() override;
  void render() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override;
  score::gfx::RenderList* renderer() const override;

  void createOutput(
      score::gfx::GraphicsApi graphicsApi, std::function<void()> onReady,
      std::function<void()> onUpdate, std::function<void()> onResize) override;
  void destroyOutput() override;

  std::shared_ptr<score::gfx::RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  SharedOutputSettings m_settings;

  QRhiReadbackResult m_readback;
  shmdata::ConsoleLogger m_logger;
};

class shmdata_output_device : public ossia::net::device_base
{
  gfx_node_base root;

public:
  shmdata_output_device(
      const SharedOutputSettings& set, std::unique_ptr<ossia::net::protocol_base> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{*this, new ShmdataOutputNode{set}, name}
  {
  }

  const gfx_node_base& get_root_node() const override { return root; }
  gfx_node_base& get_root_node() override { return root; }
};
}

namespace Gfx
{

ShmdataOutputNode::ShmdataOutputNode(const SharedOutputSettings& set)
    : OutputNode{}
    , m_settings{set}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

ShmdataOutputNode::~ShmdataOutputNode() { }
bool ShmdataOutputNode::canRender() const
{
  return bool(m_writer);
}

void ShmdataOutputNode::startRendering() { }

void ShmdataOutputNode::render()
{
  if(m_update)
    m_update();

  auto renderer = m_renderer.lock();
  if(renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb);
    rhi->endOffscreenFrame();

    int sz = m_readback.pixelSize.width() * m_readback.pixelSize.height() * 4;
    int bytes = m_readback.data.size();
    if(bytes > 0 && bytes >= sz)
      m_writer->copy_to_shm(m_readback.data.data(), sz);
  }
}

score::gfx::OutputNode::Configuration ShmdataOutputNode::configuration() const noexcept
{
  return {.manualRenderingRate = 1000. / m_settings.rate};
}

void ShmdataOutputNode::onRendererChange() { }

void ShmdataOutputNode::stopRendering() { }

void ShmdataOutputNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* ShmdataOutputNode::renderer() const
{
  return m_renderer.lock().get();
}

void ShmdataOutputNode::createOutput(
    score::gfx::GraphicsApi graphicsApi, std::function<void()> onReady,
    std::function<void()> onUpdate, std::function<void()> onResize)
{
  m_writer = std::make_unique<shmdata::Writer>(
      m_settings.path.toStdString(), m_settings.width * m_settings.height * 4,
      fmt::format(
          "video/x-raw,format=RGBA,width={},height={},framerate={}/1", m_settings.width,
          m_settings.height, int(m_settings.rate)),
      &m_logger);
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

void ShmdataOutputNode::destroyOutput()
{
  m_writer.reset();
}

std::shared_ptr<score::gfx::RenderState> ShmdataOutputNode::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNodeRenderer*
ShmdataOutputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};
  return new Gfx::InvertYRenderer{rt, const_cast<QRhiReadbackResult&>(m_readback)};
}

ShmdataOutputDevice::~ShmdataOutputDevice() { }

bool ShmdataOutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto set = m_settings.deviceSpecificSettings.value<SharedOutputSettings>();
      m_protocol = new gfx_protocol_base{plug->exec};
      m_dev = std::make_unique<shmdata_output_device>(
          set, std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          m_settings.name.toStdString());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

Device::ProtocolSettingsWidget* ShmdataOutputProtocolFactory::makeSettingsWidget()
{
  return new ShmdataOutputSettingsWidget{};
}

QString ShmdataOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Shmdata Output");
}

Device::DeviceInterface* ShmdataOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& doc,
    const score::DocumentContext& ctx)
{
  return new ShmdataOutputDevice(settings, ctx);
}

const Device::DeviceSettings&
ShmdataOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Shmdata Output";
    SharedOutputSettings set;
    set.width = 1280;
    set.height = 720;
    set.path = "/tmp/score_shm_video";
    set.rate = 60.;
    s.deviceSpecificSettings = QVariant::fromValue(set);
    return s;
  }();
  return settings;
}

ShmdataOutputSettingsWidget::ShmdataOutputSettingsWidget(QWidget* parent)
    : SharedOutputSettingsWidget{parent}
{
  m_deviceNameEdit->setText("Shmdata Out");
  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Shmdata path");

  auto helpLabel
      = new QLabel{tr("To test, use the following command: \n"
                      "$ gst-launch-1.0 shmdatasrc socket-path=<THE PATH> ! "
                      "videoconvert ! xvimagesink")};
  helpLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
  m_layout->addRow(helpLabel);

  setSettings(ShmdataOutputProtocolFactory{}.defaultSettings());
}

Device::DeviceSettings ShmdataOutputSettingsWidget::getSettings() const
{
  auto set = SharedOutputSettingsWidget::getSettings();
  set.protocol = ShmdataOutputProtocolFactory::static_concreteKey();
  return set;
}

}
#include <Gfx/Qt5CompatPop> // clang-format: keep
