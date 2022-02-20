#include "SpoutInput.hpp"
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <Spout/SpoutReceiver.h>
#include <QFormLayout>
#include <Gfx/Graph/decoders/RGBA.hpp>

#include <wobjectimpl.h>

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::Spout::InputSettings);

namespace Gfx::Spout
{
class InputDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(InputDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  ossia::net::protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

InputDevice::~InputDevice() { }

struct SpoutInputNode : score::gfx::ProcessNode
{
public:
  explicit SpoutInputNode(const InputSettings& s)
    : settings{s}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  InputSettings settings;

  virtual ~SpoutInputNode()
  {
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  class Renderer;
};

#include <Gfx/Qt5CompatPush> // clang-format: keep
class SpoutInputNode::Renderer : public score::gfx::NodeRenderer
{
public:
  Renderer(const SpoutInputNode& n): node{n} {

  }

private:
  const SpoutInputNode& node;
  Video::VideoMetadata metadata;

  // TODO refactor with VideoNodeRenderer
  score::gfx::PassMap m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  struct Material {
    float scale_w{1.0f}, scale_h{1.0f};
  };
  std::unique_ptr<score::gfx::PackedDecoder> m_gpu{};

  bool enabled{};
  ~Renderer() { }

  score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override { return { }; }
  void init(score::gfx::RenderList& renderer) override
  {
    // Initialize our rendering structures
    auto& rhi = *renderer.state.rhi;
    const auto& mesh = renderer.defaultQuad();
    if (!m_meshBuffer)
    {
      auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
      m_meshBuffer = mbuffer;
      m_idxBuffer = ibuffer;
    }

    m_processUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(score::gfx::ProcessUBO));
    m_processUBO->create();

    m_materialUBO = rhi.newBuffer(
    QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Material));
    m_materialUBO->create();

    // Initialize spout
    m_receiver.SetReceiverName(node.settings.path.toStdString().c_str());

    char sendername[256];
    uint w = 16, h = 16;
    m_receiver.SetShareMode(0);
    enabled = m_receiver.CreateReceiver(sendername, w, h);

    metadata.width = std::max((uint)1, w);
    metadata.height = std::max((uint)1, h);

    m_gpu = std::make_unique<score::gfx::PackedDecoder>(QRhiTexture::RGBA8, 4, metadata, QString{});
    createPipelines(renderer);
    m_pixels.resize(w * h * 4);
  }

  void createPipelines(score::gfx::RenderList& r)
  {
    if (m_gpu)
    {
      auto shaders = m_gpu->init(r);
      SCORE_ASSERT(m_p.empty());
      score::gfx::defaultPassesInit(
          m_p,
          this->node.output[0]->edges,
          r,
          r.defaultQuad(),
          shaders.first,
          shaders.second,
          m_processUBO,
          m_materialUBO,
          m_gpu->samplers);
    }
  }

  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    res.updateDynamicBuffer(m_processUBO, 0, sizeof(score::gfx::ProcessUBO), &this->node.standardUBO);
    Material mat;
    mat.scale_w = 1.;
    mat.scale_h = 1.;
    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    if(!enabled)
    {
      char sendername[256];
      uint w = 16, h = 16;
      enabled = m_receiver.CreateReceiver(sendername, w, h);
      if(!enabled)
        return;
    }

    SCORE_ASSERT(!m_gpu->samplers.empty());

    auto tex = m_gpu->samplers[0].texture;
    auto& rhi = *renderer.state.rhi;

    // Check the current status of the Spout remote
    bool connected{};
    QSize cursize{metadata.width, metadata.height};
    uint w = metadata.width, h = metadata.height;
    char sendername[256] = "Spout Demo Sender";

    // if(!m_receiver.CheckReceiver(sendername, w, h, connected))
    //   return;
    m_receiver.IsUpdated();

    bool mustUpload = false;
    // Check if the texture size changed
    if(w != metadata.width || h != metadata.height)
    {
      metadata.width = w;
      metadata.height = h;

      if(metadata.width > 0 && metadata.height > 0)
      {
        m_pixels.resize(w * h * 4);
        tex->release();
        tex->setPixelSize(QSize(w, h));
        tex->build();
        for(auto& pass : m_p)
          pass.second.srb->build();
        mustUpload = true;
        qDebug() << "Resized";
      }
      else
      {
        return;
      }
    }

    if(metadata.width > 0 && metadata.height > 0)
    {
      // rhi.makeThreadLocalNativeContextCurrent();
      if(m_receiver.ReceiveImage((unsigned char*)m_pixels.data(), GL_RGBA, true))
      {
        mustUpload = m_receiver.IsFrameNew();
      }
    }

    if(mustUpload)
    {
      //std::generate_n(m_pixels.data(), metadata.width * metadata.height * 4, [] { return rand(); });
      m_gpu->setPixels(res, (uint8_t*)m_pixels.data(), metadata.width * 4);
    }
    else
    {
      std::generate_n(m_pixels.data(), metadata.width * metadata.height * 4, [] { return rand(); });
      m_gpu->setPixels(res, (uint8_t*)m_pixels.data(), metadata.width * 4);
    }
  }

  void runRenderPass(
      score::gfx::RenderList& renderer,
      QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultTriangle();
    score::gfx::quadRenderPass(m_meshBuffer, m_idxBuffer, renderer, cb, edge, m_p);
  }

  void release(score::gfx::RenderList& r) override
  {
    if(enabled)
    {
      m_receiver.ReleaseReceiver();
      enabled = false;
    }

    if (m_gpu)
    {
      m_gpu->release(r);
    }

    delete m_processUBO;
    m_processUBO = nullptr;
    delete m_materialUBO;
    m_materialUBO = nullptr;

    for(auto& p : m_p)
      p.second.release();
    m_p.clear();

    m_meshBuffer = nullptr;
  }

  ::SpoutReceiver m_receiver;
  std::vector<char> m_pixels;
};
#include <Gfx/Qt5CompatPop> // clang-format: keep

score::gfx::NodeRenderer* SpoutInputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new Renderer{*this};
}



bool InputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<InputSettings>();

    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if (plug)
    {
      auto protocol = std::make_unique<simple_texture_input_protocol>();
      m_protocol = protocol.get();
      m_dev = std::make_unique<simple_texture_input_device>(
                new SpoutInputNode{set},
                &plug->exec,
                std::move(protocol),
                this->settings().name.toStdString());
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

QString InputFactory::prettyName() const noexcept
{
  return QObject::tr("Spout Input");
}

QString InputFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* InputFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface*
InputFactory::makeDevice(const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin, const score::DocumentContext& ctx)
{
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& InputFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Spout Input";
    InputSettings specif;
    specif.path = "Spout DX11 Sender";
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* InputFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* InputFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* InputFactory::makeSettingsWidget()
{
  return new InputSettingsWidget;
}

QVariant InputFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<InputSettings>(visitor);
}

void InputFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<InputSettings>(data, visitor);
}

bool InputFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

InputSettingsWidget::InputSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Spout path"), m_shmPath = new QLineEdit);
  setLayout(layout);

  setSettings(InputFactory{}.defaultSettings());
}


Device::DeviceSettings InputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = InputFactory::static_concreteKey();
  InputSettings set;
  set.path = m_shmPath->text();
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void InputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);

  const auto& set = settings.deviceSpecificSettings.value<InputSettings>();
  m_shmPath->setText(set.path);
}

}

template <>
void DataStreamReader::read(const Gfx::Spout::InputSettings& n)
{
  m_stream << n.path;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Spout::InputSettings& n)
{
  m_stream >> n.path;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Spout::InputSettings& n)
{
  obj["Path"] = n.path;
}

template <>
void JSONWriter::write(Gfx::Spout::InputSettings& n)
{
  n.path = obj["Path"].toString();
}

W_OBJECT_IMPL(Gfx::Spout::InputDevice)
