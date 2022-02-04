#include "ShmdataOutputDevice.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QtGui/private/qrhigles2_p_p.h>

#include <ossia/detail/fmt.hpp>
#include <shmdata/console-logger.hpp>
#include <shmdata/writer.hpp>
#include <QLabel>
#include <QSpinBox>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::ShmdataOutputDevice)

#include <Gfx/Qt5CompatPush> // clang-format: keep

namespace Gfx
{
struct ShmdataOutputNode : score::gfx::OutputNode
{
  explicit ShmdataOutputNode(const ShmSettings&);
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
      score::gfx::GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onUpdate,
      std::function<void()> onResize) override;
  void destroyOutput() override;

  score::gfx::RenderState* renderState() const override;
  score::gfx::OutputNodeRenderer* createRenderer(score::gfx::RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  ShmSettings m_settings;

  QRhiReadbackResult m_readback;
  shmdata::ConsoleLogger m_logger;
};

class shmdata_output_device : public ossia::net::device_base
{
  gfx_node_base root;

public:
  shmdata_output_device(
      const ShmSettings& set,
      std::unique_ptr<ossia::net::protocol_base> proto,
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


ShmdataOutputNode::ShmdataOutputNode(const ShmSettings& set)
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

void ShmdataOutputNode::startRendering()
{
}


void ShmdataOutputNode::render()
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
    rhi->endOffscreenFrame();

    int sz = m_readback.pixelSize.width() * m_readback.pixelSize.height() * 4;
    m_writer->copy_to_shm(m_readback.data.data(), sz);
  }
}

score::gfx::OutputNode::Configuration ShmdataOutputNode::configuration() const noexcept
{
  return { .manualRenderingRate = 1000. / m_settings.rate };
}

void ShmdataOutputNode::onRendererChange()
{
}

void ShmdataOutputNode::stopRendering()
{
}

void ShmdataOutputNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* ShmdataOutputNode::renderer() const
{
  return m_renderer.lock().get();
}

void ShmdataOutputNode::createOutput(
    score::gfx::GraphicsApi graphicsApi,
    std::function<void()> onReady,
    std::function<void()> onUpdate,
    std::function<void()> onResize)
{
  m_writer = std::make_unique<shmdata::Writer>(m_settings.path.toStdString(),
                                                      m_settings.width * m_settings.height * 4,
                                                      fmt::format("video/x-raw,format=RGBA,width={},height={}", m_settings.width, m_settings.height),
                                                      &m_logger);
  m_renderState = std::make_shared<score::gfx::RenderState>();
  m_update = onUpdate;

  m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
  QRhiGles2InitParams params;
  params.fallbackSurface = m_renderState->surface;
#include <Gfx/Qt5CompatPop> // clang-format: keep
  m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
#include <Gfx/Qt5CompatPush> // clang-format: keep
  m_renderState->size = QSize(m_settings.width, m_settings.height);

  auto rhi = m_renderState->rhi;
  m_texture = rhi->newTexture(
      QRhiTexture::RGBA8,
      m_renderState->size,
      1,
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

score::gfx::RenderState* ShmdataOutputNode::renderState() const
{
  return m_renderState.get();
}


score::gfx::OutputNodeRenderer* ShmdataOutputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{m_texture, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};
  return new Gfx::InvertYRenderer{rt, const_cast<QRhiReadbackResult&>(m_readback)};
}

ShmdataOutputDevice::~ShmdataOutputDevice() { }

bool ShmdataOutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      auto set = m_settings.deviceSpecificSettings.value<ShmSettings>();
      m_protocol = new gfx_protocol_base{plug->exec};
      m_dev = std::make_unique<shmdata_output_device>(
          set,
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          m_settings.name.toStdString());
    }
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

QString ShmdataOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Shmdata Output");
}

QString ShmdataOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator*
ShmdataOutputProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* ShmdataOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& doc,
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
    ShmSettings set;
    set.width = 1280;
    set.height = 720;
    set.path = "/tmp/score_shm_video";
    set.rate = 60.;
    s.deviceSpecificSettings = QVariant::fromValue(set);
    return s;
  }();
  return settings;
}

Device::AddressDialog* ShmdataOutputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* ShmdataOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* ShmdataOutputProtocolFactory::makeSettingsWidget()
{
  return new ShmdataOutputSettingsWidget;
}

QVariant ShmdataOutputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<ShmSettings>(visitor);
}

void ShmdataOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<ShmSettings>(data, visitor);
}

bool ShmdataOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

ShmdataOutputSettingsWidget::ShmdataOutputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Shmdata path"), m_shmPath = new QLineEdit);
  layout->addRow(tr("Width"), m_width = new QSpinBox);
  layout->addRow(tr("Height"), m_height = new QSpinBox);
  layout->addRow(tr("Rate"), m_rate = new QSpinBox);

  m_width->setRange(1, 16384);
  m_height->setRange(1, 16384);
  m_rate->setRange(1, 1000);

  auto helpLabel = new QLabel{tr("To test, use the following command: \n"
                                 "$ gst-launch-1.0 shmdatasrc socket-path=<THE PATH> ! videoconvert ! xvimagesink")};
  helpLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
  layout->addRow(helpLabel);


  setLayout(layout);

  setSettings(ShmdataOutputProtocolFactory{}.defaultSettings());
}


Device::DeviceSettings ShmdataOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = ShmdataOutputProtocolFactory::static_concreteKey();

  ShmSettings set;
  set.width = m_width->value();
  set.height = m_height->value();
  set.path = m_shmPath->text();
  set.rate = m_rate->value();

  s.deviceSpecificSettings = QVariant::fromValue(set);

  return s;
}

void ShmdataOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& set = settings.deviceSpecificSettings.value<ShmSettings>();
  m_shmPath->setText(set.path);
  m_width->setValue(set.width);
  m_height->setValue(set.height);
  m_rate->setValue(set.rate);
}

}
#include <Gfx/Qt5CompatPop> // clang-format: keep

template <>
void DataStreamReader::read(const Gfx::ShmSettings& n)
{
  m_stream << n.path << n.width << n.height << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::ShmSettings& n)
{
  m_stream >> n.path >> n.width >> n.height >> n.rate;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::ShmSettings& n)
{
  obj["Path"] = n.path;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  obj["Rate"] = n.rate;
}

template <>
void JSONWriter::write(Gfx::ShmSettings& n)
{
  n.path = obj["Path"].toString();
  n.width = obj["Width"].toDouble();
  n.height = obj["Height"].toDouble();
  n.rate = obj["Rate"].toDouble();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::ShmSettings);
