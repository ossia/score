#include "Sh4ltOutputDevice.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QOffscreenSurface>
#include <QSpinBox>

#include <sh4lt/logger/console.hpp>
#include <sh4lt/writer.hpp>

#include <wobjectimpl.h>

#include <sh4lt/shtype/shtype-from-gst-caps.hpp>

namespace Gfx
{
class Sh4ltOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(Sh4ltOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~Sh4ltOutputDevice();

private:
  void disconnect() override;
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

class Sh4ltOutputSettingsWidget final : public Gfx::SharedOutputSettingsWidget
{
public:
  Sh4ltOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
};

}
W_OBJECT_IMPL(Gfx::Sh4ltOutputDevice)

namespace Gfx
{
struct Sh4ltOutputNode : score::gfx::OutputNode
{
  explicit Sh4ltOutputNode(const SharedOutputSettings&);
  virtual ~Sh4ltOutputNode();

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::shared_ptr<sh4lt::Writer> m_writer{};
  int64_t m_frame_counter{0};
  int64_t m_frame_dur{0};
  bool m_hasSender{};

  void startRendering() override;
  void onRendererChange() override;
  void render() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override;
  score::gfx::RenderList* renderer() const override;

  void createOutput(score::gfx::OutputConfiguration conf) override;
  void destroyOutput() override;

  std::shared_ptr<score::gfx::RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  SharedOutputSettings m_settings;

  QRhiReadbackResult m_readback;
  std::shared_ptr<sh4lt::logger::Console> m_logger{
      std::make_shared<sh4lt::logger::Console>()};
};

class sh4lt_output_device : public ossia::net::device_base
{
  gfx_node_base root;

public:
  sh4lt_output_device(
      const SharedOutputSettings& set, std::unique_ptr<gfx_protocol_base> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{
            *this, *static_cast<gfx_protocol_base*>(m_protocol.get()),
            new Sh4ltOutputNode{set}, name}
  {
  }

  const gfx_node_base& get_root_node() const override { return root; }
  gfx_node_base& get_root_node() override { return root; }
};
}

namespace Gfx
{

Sh4ltOutputNode::Sh4ltOutputNode(const SharedOutputSettings& set)
    : OutputNode{}
    , m_settings{set}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

Sh4ltOutputNode::~Sh4ltOutputNode() { }
bool Sh4ltOutputNode::canRender() const
{
  return bool(m_writer);
}

void Sh4ltOutputNode::startRendering() { }

void Sh4ltOutputNode::render()
{
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
    {
      m_writer->copy_to_shm(
          m_readback.data.data(), sz, m_frame_dur * m_frame_counter, m_frame_dur);
      ++m_frame_counter;
    }
  }
}

score::gfx::OutputNode::Configuration Sh4ltOutputNode::configuration() const noexcept
{
  return {.manualRenderingRate = 1000. / m_settings.rate};
}

void Sh4ltOutputNode::onRendererChange() { }

void Sh4ltOutputNode::stopRendering() { }

void Sh4ltOutputNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* Sh4ltOutputNode::renderer() const
{
  return m_renderer.lock().get();
}

void Sh4ltOutputNode::createOutput(score::gfx::OutputConfiguration conf)
{
  m_writer = std::make_unique<sh4lt::Writer>(
      sh4lt::shtype::shtype_from_gst_caps(
          fmt::format(
              "video/x-raw, format=(string)RGBA, width=(int){}, height=(int){}, "
              "framerate={}/1",
              m_settings.width, m_settings.height, int(m_settings.rate)),
          m_settings.path.toStdString()),
      m_settings.width * m_settings.height * 4, m_logger);
  m_frame_dur = 1e9 / m_settings.rate;
  m_renderState = std::make_shared<score::gfx::RenderState>();

  m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
  QRhiGles2InitParams params;
  params.fallbackSurface = m_renderState->surface;
  score::GLCapabilities caps;
  caps.setupFormat(params.format);
  m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
  m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
  m_renderState->outputSize = m_renderState->renderSize;
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

  conf.onReady();
}

void Sh4ltOutputNode::destroyOutput()
{
  m_writer.reset();
}

std::shared_ptr<score::gfx::RenderState> Sh4ltOutputNode::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNodeRenderer*
Sh4ltOutputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};
  return new Gfx::InvertYRenderer{
      *this, rt, const_cast<QRhiReadbackResult&>(m_readback)};
}

Sh4ltOutputDevice::~Sh4ltOutputDevice() { }

void Sh4ltOutputDevice::disconnect()
{
  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

bool Sh4ltOutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto set = m_settings.deviceSpecificSettings.value<SharedOutputSettings>();
      m_protocol = new gfx_protocol_base{plug->exec};
      m_dev = std::make_unique<sh4lt_output_device>(
          set, std::unique_ptr<gfx_protocol_base>(m_protocol),
          m_settings.name.toStdString());
      deviceChanged(nullptr, m_dev.get());
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

Device::ProtocolSettingsWidget* Sh4ltOutputProtocolFactory::makeSettingsWidget()
{
  return new Sh4ltOutputSettingsWidget{};
}

QString Sh4ltOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Sh4lt Output");
}

QUrl Sh4ltOutputProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/sh4lt-device.html");
}

Device::DeviceInterface* Sh4ltOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& doc,
    const score::DocumentContext& ctx)
{
  return new Sh4ltOutputDevice(settings, ctx);
}

const Device::DeviceSettings&
Sh4ltOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Sh4lt Output";
    SharedOutputSettings set;
    set.width = 1280;
    set.height = 720;
    set.path = "score_output";
    set.rate = 60.;
    s.deviceSpecificSettings = QVariant::fromValue(set);
    return s;
  }();
  return settings;
}

Sh4ltOutputSettingsWidget::Sh4ltOutputSettingsWidget(QWidget* parent)
    : SharedOutputSettingsWidget{parent}
{
  m_deviceNameEdit->setText("Sh4lt Out");
  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Sh4lt path");

  auto helpLabel
      = new QLabel{tr("To test, use the following command: \n"
                      "$ gst-launch-1.0 sh4ltsrc label=<THE LABEL> ! "
                      "videoconvert ! xvimagesink")};
  helpLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
  m_layout->addRow(helpLabel);

  setSettings(Sh4ltOutputProtocolFactory{}.defaultSettings());
}

Device::DeviceSettings Sh4ltOutputSettingsWidget::getSettings() const
{
  auto set = SharedOutputSettingsWidget::getSettings();
  set.protocol = Sh4ltOutputProtocolFactory::static_concreteKey();
  return set;
}

}
