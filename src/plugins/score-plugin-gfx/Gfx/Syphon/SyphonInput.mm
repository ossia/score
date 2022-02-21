#include "SyphonInput.hpp"
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Syphon/SyphonHelpers.hpp>
#include <Syphon/SyphonClient.h>
#include <Syphon/SyphonOpenGLImage.h>
#include <Syphon/SyphonServerDirectory.h>
#include <QtPlatformHeaders/QCocoaNativeContext>
#include <QOpenGLContext>
#include <QFormLayout>
#include <QApplication>
#include <QLabel>

#include <wobjectimpl.h>
#include <QtGui/private/qrhigles2_p_p.h>

namespace Gfx::Syphon
{
struct SyphonInputNode : score::gfx::ProcessNode
{
public:
  explicit SyphonInputNode(const InputSettings& s)
    : settings{s}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  InputSettings settings;

  virtual ~SyphonInputNode()
  {
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  class Renderer;
};

#include <Gfx/Qt5CompatPush> // clang-format: keep
class SyphonInputNode::Renderer : public score::gfx::NodeRenderer
{
public:
  explicit Renderer(const SyphonInputNode& n): node{n} { }

private:
  const SyphonInputNode& node;
  Video::VideoMetadata metadata;

  // TODO refactor with VideoNodeRenderer
  score::gfx::PassMap m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  struct Material {
    float scale_w{1.0f}, scale_h{1.0f};
    float width{1.f}, height{1.f};
  } material;
  std::unique_ptr<score::gfx::PackedRectDecoder> m_gpu{};

  bool enabled{};
  ~Renderer() { }

  void openServer(QRhi& rhi)
  {
    enabled = false;
    // Need pool
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    SyphonServerDirectory *ssd = [SyphonServerDirectory sharedDirectory];
    NSArray *servers = [ssd serversMatchingName:NULL appName:NULL];
    if (servers.count != 0)
    {

      NSDictionary *desc = nullptr;// find_by_uuid(servers, s->uuid);
      if (!desc) {
        desc = servers[0];
        /*
          if (![s->uuid isEqualToString:
                    desc[SyphonServerDescriptionUUIDKey]]) {
            s->uuid_changed = true;
          }
          */
      }

      m_receiver = [[SYPHON_CLIENT_UNIQUE_CLASS_NAME alloc]
          initWithServerDescription:desc
          context: nativeContext(rhi)
          options:NULL
          newFrameHandler:NULL
      ];
      enabled = true;
    }
    [pool drain];
  }

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

    // Initialize syphon
    openServer(rhi);
    int w = 16, h = 16;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    SyphonOpenGLImage* img{};
    if(enabled)
    {
      if(img = [m_receiver newFrameImage]) {
        NSSize sz = img.textureSize;
        w = sz.width;
        h = sz.height;
      }
    }

    metadata.width = std::max((int)1, w);
    metadata.height = std::max((int)1, h);
    material.width = metadata.width;
    material.height = metadata.height;

    m_gpu = std::make_unique<score::gfx::PackedRectDecoder>(QRhiTexture::RGBA8, 4, metadata, QString{});
    createPipelines(renderer);
    m_pixels.resize(w * h * 4);

    if(img)
    {
      rebuildTexture(img);
      [img release];
    }

    [pool drain];
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

  void rebuildTexture(SyphonOpenGLImage* img)
  {
    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;

    QRhiTexture::NativeTexture tt;
    tt.layout = 0;
    tt.object = &currentTex;

    tex->release();
    NSSize sz = img.textureSize;
    tex->setPixelSize(QSize(sz.width, sz.height));
    tex->buildFrom(tt);

    if(auto t = dynamic_cast<QGles2Texture*>(tex))
    {
      t->target = GL_TEXTURE_RECTANGLE;
      t->gltype = GL_UNSIGNED_SHORT;
      t->glintformat = GL_BGRA;
      t->glsizedintformat = GL_BGRA;
      t->glformat = GL_BGRA;
      t->gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
    }
    for(auto& pass : m_p)
      pass.second.srb->build();
  }

  GLuint currentTex = 0;
  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if(!enabled)
    {
      auto& rhi = *renderer.state.rhi;
      openServer(rhi);
    }

    if(!m_receiver.hasNewFrame)
    {
      return;
    }

    auto& rhi = *renderer.state.rhi;

    // Check the current status of the Syphon remote
    bool connected{};

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    auto img = [m_receiver newFrameImage];
    if(!img) {
      return;
    }

    NSSize sz = img.textureSize;
    if(currentTex != img.textureName || sz.width != metadata.width || sz.height != metadata.height)
    {
      metadata.width = std::max(1., sz.width);
      metadata.height = std::max(1., sz.height);
      material.width = metadata.width;
      material.height = metadata.height;

      currentTex = img.textureName;
      rebuildTexture(img);
    }

    [img release];
    [pool drain];

    res.updateDynamicBuffer(m_processUBO, 0, sizeof(score::gfx::ProcessUBO), &this->node.standardUBO);
    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &material);
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
      //m_receiver.ReleaseReceiver();
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

  SyphonOpenGLClient* m_receiver{};
  std::vector<unsigned char> m_pixels;
};
#include <Gfx/Qt5CompatPop> // clang-format: keep

score::gfx::NodeRenderer* SyphonInputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new Renderer{*this};
}

class InputDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(InputDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice(){ }

private:
  bool reconnect() override
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
                  new SyphonInputNode{set},
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
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  ossia::net::protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

QString InputFactory::prettyName() const noexcept
{
  return QObject::tr("Syphon Input");
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
    s.name = "Syphon In";
    InputSettings specif;
    specif.path = "Syphon Demo Sender";
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
  m_deviceNameEdit->setText("Syphon In");

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
W_OBJECT_IMPL(Gfx::Syphon::InputDevice)
