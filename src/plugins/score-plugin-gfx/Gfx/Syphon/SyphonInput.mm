#include "SyphonInput.hpp"
#include <QApplication>
#include <QFormLayout>
#include <QLabel>
#include <QOpenGLContext>
#include <QUrl>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Syphon/SyphonHelpers.hpp>
#include <Syphon/SyphonClient.h>
#include <Syphon/SyphonOpenGLClient.h>
#include <Syphon/SyphonOpenGLImage.h>
#include <Syphon/SyphonServerDirectory.h>

#include <wobjectimpl.h>
#include <score/gfx/QRhiGles2.hpp>

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

class SyphonInputNode::Renderer : public score::gfx::NodeRenderer
{
public:
    explicit Renderer(const SyphonInputNode &n)
        : score::gfx::NodeRenderer{n}
        , node{n}
    {}

private:
  const SyphonInputNode& node;
  Video::VideoMetadata metadata;

  // TODO refactor with VideoNodeRenderer
  score::gfx::PassMap m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  score::gfx::VideoMaterialUBO material;
  std::unique_ptr<score::gfx::PackedRectDecoder> m_gpu{};

  bool enabled{};
  ~Renderer() { }

  NSDictionary* findServer(NSArray* servers, QString uuid)
  {
    NSString* str = uuid.toNSString();

    for(int i = 0; i < servers.count; ++i)
    {
      NSDictionary* s = servers[i];
      NSString* uuid = s[SyphonServerDescriptionUUIDKey];
      bool ok = [uuid isEqualToString: str];
      if(ok)
      {
        return s;
      }
    }

    return nullptr;
  }

  void openServer(QRhi& rhi)
  {
    enabled = false;
    // Need pool

    SyphonServerDirectory *ssd = [SyphonServerDirectory sharedDirectory];
    NSArray *servers = [ssd serversMatchingName:NULL appName:NULL];
    if (servers.count != 0)
    {
      if (NSDictionary *desc = findServer(servers, node.settings.path))
      {
        m_receiver = [[SyphonOpenGLClient alloc]
            initWithServerDescription:desc
            context: nativeContext(rhi)
            options:NULL
            newFrameHandler:NULL
        ];
      }
      enabled = true;
    }
  }

  score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override { return { }; }
  void init(score::gfx::RenderList &renderer,
            QRhiResourceUpdateBatch &res,
            score::gfx::Edge *edge) override
  {
    // Initialize our rendering structures
    auto& rhi = *renderer.state.rhi;
    const auto& mesh = renderer.defaultTriangle();
    if (!m_meshBuffer)
    {
      auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh, res);
      m_meshBuffer = mbuffer;
      m_idxBuffer = ibuffer;
    }

    m_processUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(score::gfx::ProcessUBO));
    m_processUBO->create();

    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(score::gfx::VideoMaterialUBO));
    m_materialUBO->create();

    // Initialize syphon
    openServer(rhi);
    int w = 16, h = 16;

    SyphonOpenGLImage* img{};
    if(enabled)
    {
      if((img = [m_receiver newFrameImage])) {
        NSSize sz = img.textureSize;
        w = sz.width;
        h = sz.height;
        currentTex = img.textureName;
      }
    }

    metadata.width = std::max((int)1, w);
    metadata.height = std::max((int)1, h);
    material.textureSize[0] = w;
    material.textureSize[1] = h;
    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);

    m_gpu = std::make_unique<score::gfx::PackedRectDecoder>(QRhiTexture::RGBA8, 4, metadata, QString{});
    createPipelines(renderer);

    if(img)
    {
      rebuildTexture(img);
    }
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
          r.defaultTriangle(),
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
    tt.object = currentTex;

    tex->destroy();
    NSSize sz = img.textureSize;
    tex->setPixelSize(QSize(sz.width, sz.height));
    tex->setFormat(QRhiTexture::Format::BGRA8);
    tex->setFlags(tex->flags() | QRhiTexture::TextureRectangleGL);
    tex->createFrom(tt);

    // FIXME how to ensure this ?
    auto t = static_cast<QGles2Texture*>(tex);
    {
      t->target = GL_TEXTURE_RECTANGLE;
      t->gltype = GL_UNSIGNED_SHORT;
      t->glintformat = GL_BGRA;
      t->glsizedintformat = GL_BGRA;
      t->glformat = GL_BGRA;
      t->gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
    }
    for(auto& pass : m_p)
      pass.second.srb->create();
  }

  GLuint currentTex = 0;
  void update(score::gfx::RenderList &renderer,
              QRhiResourceUpdateBatch &res,
              score::gfx::Edge *e) override
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

    auto img = [m_receiver newFrameImage];
    if(!img) {
      return;
    }

    NSSize sz = img.textureSize;
    if(currentTex != img.textureName || sz.width != metadata.width || sz.height != metadata.height)
    {
      metadata.width = std::max(1., sz.width);
      metadata.height = std::max(1., sz.height);
      material.scale[0] = 1.f;
      material.scale[1] = 1.f;
      material.textureSize[0] = metadata.width;
      material.textureSize[1] = metadata.height;

      currentTex = img.textureName;
      rebuildTexture(img);
    }

    res.updateDynamicBuffer(m_processUBO, 0, sizeof(score::gfx::ProcessUBO), &this->node.standardUBO);
    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);
  }

  void runRenderPass(
      score::gfx::RenderList& renderer,
      QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultTriangle();
    score::gfx::defaultRenderPass(renderer, mesh, {.mesh = m_meshBuffer, .index = m_idxBuffer}, cb, edge, m_p);
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
};

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

QUrl InputFactory::manual() const noexcept
{
    return QUrl("https://ossia.io/score-docs/devices/syphon-device.html");
}

class SyphonEnumerator : public Device::DeviceEnumerator
{
public:
  SyphonEnumerator()
  {
  }

  void registerServer(NSDictionary *desc,
                      std::function<void(const QString &, const Device::DeviceSettings &)> f) const
  {
    Device::DeviceSettings set;

    QString name = QString::fromNSString(desc[SyphonServerDescriptionNameKey]);
    QString appname = QString::fromNSString(desc[SyphonServerDescriptionAppNameKey]);
    QString uid = QString::fromNSString(desc[SyphonServerDescriptionUUIDKey]);

    if(name.isEmpty())
      name = appname;
    if(name.isEmpty())
      name = "Syphon In";

    set.name = name;
    set.protocol = InputFactory::static_concreteKey();

    SharedInputSettings specif;
    specif.path = uid;
    set.deviceSpecificSettings = QVariant::fromValue(specif);

    f(set.name, set);
  }

  void enumerate(std::function<void(const QString &, const Device::DeviceSettings &)> f) const override
  {
    auto ssd = [SyphonServerDirectory sharedDirectory];
    NSArray *servers = [ssd serversMatchingName:NULL appName:NULL];
    for(int i = 0; i < servers.count; i++)
    {
      registerServer(servers[i], f);
    }
  }
};


Device::DeviceEnumerators InputFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Sources", new SyphonEnumerator}};
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
