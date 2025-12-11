#include "SpoutInput.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QUrl>

#include <Spout/SpoutReceiver.h>

#include <wobjectimpl.h>

#include <set>

namespace Gfx::Spout
{
class InputSettingsWidget final : public SharedInputSettingsWidget
{
public:
  explicit InputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
};

struct SpoutInputNode : score::gfx::ProcessNode
{
public:
  explicit SpoutInputNode(const InputSettings& s)
      : settings{s}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  InputSettings settings;

  virtual ~SpoutInputNode() { }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  class Renderer;
};

class SpoutInputNode::Renderer : public score::gfx::NodeRenderer
{
public:
  explicit Renderer(const SpoutInputNode& n)
      : score::gfx::NodeRenderer{n}
      , node{n}
  {
  }

private:
  const SpoutInputNode& node;
  Video::VideoMetadata metadata;

  // TODO refactor with VideoNodeRenderer
  score::gfx::PassMap m_p;
  score::gfx::MeshBuffers m_meshBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  score::gfx::VideoMaterialUBO material;
  std::unique_ptr<score::gfx::GPUVideoDecoder> m_gpu{};

  // Spout receiver
  ::SpoutReceiver m_receiver;

  bool enabled{};

  ~Renderer() { }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    // Initialize our rendering structures
    auto& rhi = *renderer.state.rhi;
    const auto& mesh = renderer.defaultTriangle();
    if(m_meshBuffer.buffers.empty())
    {
      m_meshBuffer = renderer.initMeshBuffer(mesh, res);
    }

    m_processUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(score::gfx::ProcessUBO));
    m_processUBO->create();

    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
        sizeof(score::gfx::VideoMaterialUBO));
    m_materialUBO->create();

    // Initialize spout receiver
    m_receiver.SetReceiverName(node.settings.path.toStdString().c_str());

    // Make sure we have a valid OpenGL context
    rhi.makeThreadLocalNativeContextCurrent();

    // First call ReceiveTexture() with no texture to connect and get sender info
    // This is how Spout examples detect the initial sender size
    unsigned int w = 0, h = 0;
    if(m_receiver.ReceiveTexture())
    {
      w = m_receiver.GetSenderWidth();
      h = m_receiver.GetSenderHeight();
      enabled = true;
    }

    // Use reasonable defaults if no sender found yet
    if(w == 0 || h == 0)
    {
      w = 1280;
      h = 720;
      enabled = false;
    }

    metadata.width = w;
    metadata.height = h;

    // Use PackedDecoder (standard 2D textures, Spout uses GL_TEXTURE_2D)
    m_gpu = std::make_unique<score::gfx::PackedDecoder>(
        QRhiTexture::RGBA8, 4, metadata, QString{}, true);
    createPipelines(renderer);

    material.textureSize[0] = metadata.width;
    material.textureSize[1] = metadata.height;
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);
  }

  void createPipelines(score::gfx::RenderList& r)
  {
    if(m_gpu)
    {
      auto shaders = m_gpu->init(r);
      SCORE_ASSERT(m_p.empty());
      score::gfx::defaultPassesInit(
          m_p, this->node.output[0]->edges, r, r.defaultTriangle(), shaders.first,
          shaders.second, m_processUBO, m_materialUBO, m_gpu->samplers);
    }
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    auto& rhi = *renderer.state.rhi;

    // Make sure we're in the right GL context for Spout operations
    rhi.makeThreadLocalNativeContextCurrent();

    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;
    auto gltex = static_cast<QGles2Texture*>(tex);

    // First, call ReceiveTexture() with no args to connect and check for updates
    // This doesn't copy any pixels, just updates connection state
    if(!m_receiver.ReceiveTexture())
    {
      // No sender available
      enabled = false;
      return;
    }

    enabled = true;

    // Check if sender size changed - IsUpdated() returns true when sender changes
    // We MUST resize our texture BEFORE calling ReceiveTexture with texture ID
    if(m_receiver.IsUpdated())
    {
      unsigned int w = m_receiver.GetSenderWidth();
      unsigned int h = m_receiver.GetSenderHeight();

      if(w > 0 && h > 0 && (w != metadata.width || h != metadata.height))
      {
        metadata.width = w;
        metadata.height = h;
        material.scale[0] = 1.f;
        material.scale[1] = 1.f;
        material.textureSize[0] = metadata.width;
        material.textureSize[1] = metadata.height;

        // Resize our texture to match sender
        tex->destroy();
        tex->setPixelSize(QSize(w, h));
        tex->create();

        // Update internal GL texture state
        gltex->specified = true;

        for(auto& pass : m_p)
          pass.second.srb->create();
      }
    }

    // Now copy the texture data - our texture is correctly sized
    GLuint texId = gltex->texture;
    m_receiver.ReceiveTexture(texId, GL_TEXTURE_2D);

    res.updateDynamicBuffer(
        m_processUBO, 0, sizeof(score::gfx::ProcessUBO), &this->node.standardUBO);
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultTriangle();
    score::gfx::defaultRenderPass(renderer, mesh, m_meshBuffer, cb, edge, m_p);
  }

  void release(score::gfx::RenderList& r) override
  {
    if(enabled)
    {
      m_receiver.ReleaseReceiver();
      enabled = false;
    }

    if(m_gpu)
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

    m_meshBuffer.buffers.clear();
  }
};

score::gfx::NodeRenderer*
SpoutInputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new Renderer{*this};
}

class InputDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(InputDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice() { }

private:
  void disconnect() override
  {
    Gfx::GfxInputDevice::disconnect();
    auto prev = std::move(m_dev);
    m_dev = {};
    deviceChanged(prev.get(), nullptr);
  }

  bool reconnect() override
  {
    disconnect();

    try
    {
      auto set = this->settings().deviceSpecificSettings.value<InputSettings>();

      auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
      if(plug)
      {
        auto protocol = std::make_unique<simple_texture_input_protocol>();
        m_protocol = protocol.get();
        m_dev = std::make_unique<simple_texture_input_device>(
            new SpoutInputNode{set}, &plug->exec, std::move(protocol),
            this->settings().name.toStdString());
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
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  ossia::net::protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

QString InputFactory::prettyName() const noexcept
{
  return QObject::tr("Spout Input");
}

QUrl InputFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/spout-device.html");
}

class SpoutEnumerator : public Device::DeviceEnumerator
{
  mutable spoutSenderNames m_senders;

public:
  SpoutEnumerator() { }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    std::set<std::string> senders;
    if(!m_senders.GetSenderNames(&senders))
      return;

    for(auto& s : senders)
    {
      Device::DeviceSettings set;
      set.protocol = InputFactory::static_concreteKey();
      set.name = QString::fromStdString(s);

      SharedInputSettings specif;
      specif.path = set.name;
      set.deviceSpecificSettings = QVariant::fromValue(specif);
      f(set.name, set);
    }
  }
};

Device::DeviceEnumerators
InputFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Sources", new SpoutEnumerator}};
}

Device::DeviceInterface* InputFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& InputFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Spout In";
    InputSettings specif;
    specif.path = "Spout Demo Sender";
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* InputFactory::makeSettingsWidget()
{
  return new InputSettingsWidget;
}

InputSettingsWidget::InputSettingsWidget(QWidget* parent)
    : SharedInputSettingsWidget(parent)
{
  m_deviceNameEdit->setText("Spout In");

  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Identifier");
  setSettings(InputFactory{}.defaultSettings());
}

Device::DeviceSettings InputSettingsWidget::getSettings() const
{
  auto set = SharedInputSettingsWidget::getSettings();
  set.protocol = InputFactory::static_concreteKey();
  return set;
}

}
W_OBJECT_IMPL(Gfx::Spout::InputDevice)
