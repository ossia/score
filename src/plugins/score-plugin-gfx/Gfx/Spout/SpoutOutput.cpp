#include "SpoutOutput.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>
#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <rhi/qrhi.h>

#include <QFormLayout>
#include <QLabel>
#include <QOffscreenSurface>
#include <QUrl>

#include <Spout/SpoutSender.h>
#include <Spout/SpoutDirectX.h>

// Include QRhi D3D11/D3D12 private headers for native texture access
#include <private/qrhid3d11_p.h>
#include <private/qrhid3d12_p.h>

// D3D11On12 for D3D12 interop
#include <d3d11on12.h>
#pragma comment(lib, "d3d11.lib")

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
    if(m_created)
      return;

    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        startRenderingOpenGL();
        break;
      case QRhi::D3D11:
        startRenderingD3D11();
        break;
      case QRhi::D3D12:
        startRenderingD3D12();
        break;
      default:
        break;
    }
  }

  void startRenderingOpenGL()
  {
    m_created = m_spout->CreateSender(
        m_settings.path.toStdString().c_str(), m_settings.width, m_settings.height);

    if(m_created)
    {
      m_spout->EnableFrameSync(true);
    }
  }

  void startRenderingD3D11()
  {
    if(!m_renderState || !m_renderState->rhi)
      return;

    auto nativeHandles = static_cast<const QRhiD3D11NativeHandles*>(
        m_renderState->rhi->nativeHandles());
    if(!nativeHandles || !nativeHandles->dev)
      return;

    auto device = static_cast<ID3D11Device*>(nativeHandles->dev);

    // Create shared D3D11 texture for Spout
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_settings.width;
    texDesc.Height = m_settings.height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_sharedTexture);
    if(FAILED(hr) || !m_sharedTexture)
      return;

    // Get the shared handle
    IDXGIResource* dxgiResource = nullptr;
    hr = m_sharedTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
    if(SUCCEEDED(hr) && dxgiResource)
    {
      dxgiResource->GetSharedHandle(&m_shareHandle);
      dxgiResource->Release();
    }

    if(!m_shareHandle)
    {
      m_sharedTexture->Release();
      m_sharedTexture = nullptr;
      return;
    }

    // Register the sender
    m_created = m_dxSenderNames.CreateSender(
        (char*)m_settings.path.toStdString().c_str(), m_settings.width, m_settings.height,
        m_shareHandle, DXGI_FORMAT_B8G8R8A8_UNORM);
  }

  void startRenderingD3D12()
  {
    if(!m_d3d11On12Device || !m_d3d11Device || !m_d3d11Context)
      return;

    // Create shared D3D11 texture for Spout using the D3D11On12 device
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_settings.width;
    texDesc.Height = m_settings.height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = m_d3d11Device->CreateTexture2D(&texDesc, nullptr, &m_sharedTexture);
    if(FAILED(hr) || !m_sharedTexture)
      return;

    // Get the shared handle
    IDXGIResource* dxgiResource = nullptr;
    hr = m_sharedTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
    if(SUCCEEDED(hr) && dxgiResource)
    {
      dxgiResource->GetSharedHandle(&m_shareHandle);
      dxgiResource->Release();
    }

    if(!m_shareHandle)
    {
      m_sharedTexture->Release();
      m_sharedTexture = nullptr;
      return;
    }

    // Register the sender
    m_created = m_dxSenderNames.CreateSender(
        (char*)m_settings.path.toStdString().c_str(), m_settings.width, m_settings.height,
        m_shareHandle, DXGI_FORMAT_B8G8R8A8_UNORM);
  }

  void onRendererChange() override { }
  bool canRender() const override
  {
    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        return bool(m_spout);
      case QRhi::D3D11:
        return m_sharedTexture != nullptr;
      case QRhi::D3D12:
        return m_d3d11On12Device != nullptr && m_sharedTexture != nullptr;
      default:
        return false;
    }
  }

  void render() override
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

      if(!m_created)
        return;

      switch(m_backend)
      {
        case QRhi::OpenGLES2:
          renderOpenGL(rhi);
          break;
        case QRhi::D3D11:
          renderD3D11(rhi);
          break;
        case QRhi::D3D12:
          renderD3D12(rhi);
          break;
        default:
          break;
      }
    }
  }

  void renderOpenGL(QRhi* rhi)
  {
    rhi->makeThreadLocalNativeContextCurrent();
    rhi->finish();

    auto tex = static_cast<QGles2Texture*>(m_texture)->texture;
    m_spout->SendTexture(tex, GL_TEXTURE_2D, m_settings.width, m_settings.height);
    m_spout->SetFrameSync(m_settings.path.toStdString().c_str());
  }

  void renderD3D11(QRhi* rhi)
  {
    rhi->finish();

    auto nativeHandles
        = static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
    if(!nativeHandles || !nativeHandles->context)
      return;

    auto context = static_cast<ID3D11DeviceContext*>(nativeHandles->context);
    auto d3dtex = static_cast<QD3D11Texture*>(m_texture);

    if(d3dtex->tex && m_sharedTexture)
    {
      // Copy from our render texture to the shared Spout texture
      context->CopyResource(m_sharedTexture, d3dtex->tex);
    }
  }

  void renderD3D12(QRhi* rhi)
  {
    if(!m_d3d11On12Device || !m_d3d11Context || !m_sharedTexture)
      return;

    rhi->finish();

    // Get the native D3D12 resource from QRhiTexture
    auto nativeTex = m_texture->nativeTexture();
    auto d3d12Resource = reinterpret_cast<ID3D12Resource*>(nativeTex.object);
    if(!d3d12Resource)
      return;

    // Wrap the D3D12 texture for D3D11 access if not already wrapped
    if(!m_wrappedTexture)
    {
      D3D11_RESOURCE_FLAGS d3d11Flags = {};
      d3d11Flags.BindFlags = D3D11_BIND_SHADER_RESOURCE;

      HRESULT hr = m_d3d11On12Device->CreateWrappedResource(
          d3d12Resource, &d3d11Flags, D3D12_RESOURCE_STATE_RENDER_TARGET,
          D3D12_RESOURCE_STATE_RENDER_TARGET, IID_PPV_ARGS(&m_wrappedTexture));

      if(FAILED(hr))
      {
        m_wrappedTexture = nullptr;
        return;
      }
    }

    // Acquire the wrapped resource for D3D11 use
    m_d3d11On12Device->AcquireWrappedResources(&m_wrappedTexture, 1);

    // Copy from wrapped D3D12 texture to shared D3D11 texture
    m_d3d11Context->CopyResource(m_sharedTexture, m_wrappedTexture);

    // Release the wrapped resource back to D3D12
    m_d3d11On12Device->ReleaseWrappedResources(&m_wrappedTexture, 1);

    // Flush D3D11 commands
    m_d3d11Context->Flush();
  }

  void stopRendering() override { }

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }

  score::gfx::RenderList* renderer() const override { return m_renderer.lock().get(); }

  void createOutput(
      score::gfx::GraphicsApi graphicsApi, std::function<void()> onReady,
      std::function<void()> onUpdate, std::function<void()> onResize) override
  {
    m_renderState = std::make_shared<score::gfx::RenderState>();
    m_update = onUpdate;

    // Choose backend based on requested API
    switch(graphicsApi)
    {
      case score::gfx::GraphicsApi::D3D11:
        createOutputD3D11();
        break;
      case score::gfx::GraphicsApi::D3D12:
        createOutputD3D12();
        break;
      case score::gfx::GraphicsApi::OpenGL:
      default:
        createOutputOpenGL();
        break;
    }

    auto rhi = m_renderState->rhi;
    if(!rhi)
    {
      qWarning() << "Failed to create QRhi for Spout output";
      return;
    }

    // Use BGRA for D3D backends, RGBA for OpenGL
    auto format = (m_backend == QRhi::D3D11 || m_backend == QRhi::D3D12)
                      ? QRhiTexture::BGRA8
                      : QRhiTexture::RGBA8;

    m_texture = rhi->newTexture(
        format, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    onReady();
  }

  void createOutputOpenGL()
  {
    m_backend = QRhi::OpenGLES2;
    m_spout = std::make_shared<SpoutSender>();

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
  }

  void createOutputD3D11()
  {
    m_backend = QRhi::D3D11;

    QRhiD3D11InitParams params;
    m_renderState->rhi = QRhi::create(QRhi::D3D11, &params, {});
    m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->api = score::gfx::GraphicsApi::D3D11;
    m_renderState->version = Gfx::Settings::shaderVersionForAPI(score::gfx::GraphicsApi::D3D11);
  }

  void createOutputD3D12()
  {
    m_backend = QRhi::D3D12;

    QRhiD3D12InitParams params;
    m_renderState->rhi = QRhi::create(QRhi::D3D12, &params, {});
    m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->api = score::gfx::GraphicsApi::D3D12;
    m_renderState->version = Gfx::Settings::shaderVersionForAPI(score::gfx::GraphicsApi::D3D12);

    // Get D3D12 device and command queue from QRhi
    if(m_renderState->rhi)
    {
      auto nativeHandles = static_cast<const QRhiD3D12NativeHandles*>(
          m_renderState->rhi->nativeHandles());
      if(nativeHandles && nativeHandles->dev && nativeHandles->commandQueue)
      {
        auto d3d12Device = static_cast<ID3D12Device*>(nativeHandles->dev);
        IUnknown* cmdQueue = static_cast<IUnknown*>(nativeHandles->commandQueue);

        // Create D3D11On12 device for interop
        UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = D3D11On12CreateDevice(
            d3d12Device, d3d11DeviceFlags, nullptr, 0, &cmdQueue, 1, 0,
            &m_d3d11Device, &m_d3d11Context, nullptr);

        if(SUCCEEDED(hr) && m_d3d11Device)
        {
          // Get the D3D11On12Device interface
          m_d3d11Device->QueryInterface(
              __uuidof(ID3D11On12Device),
              reinterpret_cast<void**>(&m_d3d11On12Device));
        }
      }
    }
  }

  void destroyOutput() override
  {
    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        if(m_spout)
          m_spout->ReleaseSender();
        m_spout.reset();
        break;
      case QRhi::D3D11:
      {
        m_dxSenderNames.ReleaseSenderName(m_settings.path.toStdString().c_str());
        if(m_sharedTexture)
        {
          m_sharedTexture->Release();
          m_sharedTexture = nullptr;
        }
        m_shareHandle = nullptr;
        break;
      }
      case QRhi::D3D12:
      {
        m_dxSenderNames.ReleaseSenderName(m_settings.path.toStdString().c_str());

        // Release wrapped texture first
        if(m_wrappedTexture)
        {
          m_wrappedTexture->Release();
          m_wrappedTexture = nullptr;
        }
        // Release shared texture
        if(m_sharedTexture)
        {
          m_sharedTexture->Release();
          m_sharedTexture = nullptr;
        }
        m_shareHandle = nullptr;
        // Release D3D11On12 resources
        if(m_d3d11On12Device)
        {
          m_d3d11On12Device->Release();
          m_d3d11On12Device = nullptr;
        }
        if(m_d3d11Context)
        {
          m_d3d11Context->Release();
          m_d3d11Context = nullptr;
        }
        if(m_d3d11Device)
        {
          m_d3d11Device->Release();
          m_d3d11Device = nullptr;
        }
        break;
      }
      default:
        break;
    }
    m_created = false;
  }

  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    class SpoutRenderer : public score::gfx::OutputNodeRenderer
    {
    public:
      SpoutRenderer(const score::gfx::RenderState& state, const SpoutNode& parent)
          : score::gfx::OutputNodeRenderer{parent}
      {
        m_rt.texture = parent.m_texture;
        m_rt.renderTarget = parent.m_renderTarget;
        m_rt.renderPass = state.renderPassDescriptor;
      }

      score::gfx::TextureRenderTarget
      renderTargetForInput(const score::gfx::Port& p) override
      {
        return m_rt;
      }
      void finishFrame(score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
                       QRhiResourceUpdateBatch*& res) override
      {
      }
      void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
      void update(
          score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
          score::gfx::Edge* edge) override
      {
      }
      void release(score::gfx::RenderList&) override { }

    private:
      score::gfx::TextureRenderTarget m_rt;
    };

    score::gfx::TextureRenderTarget m_rt;

    m_rt.texture = m_texture;
    m_rt.renderTarget = m_renderTarget;
    m_rt.renderPass = r.state.renderPassDescriptor;

    switch(m_backend)
    {
      default:
      case QRhi::OpenGLES2:
        return new Gfx::BasicRenderer{m_rt, r.state, *this};
      case QRhi::D3D11:
      case QRhi::D3D12:
        return new Gfx::ScaledRenderer{m_rt, r.state, *this};
    }
  }

  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / m_settings.rate};
  }

private:
  SharedOutputSettings m_settings;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::function<void()> m_update;
  std::shared_ptr<score::gfx::RenderState> m_renderState{};

  // OpenGL backend
  std::shared_ptr<SpoutSender> m_spout{};

  // Common to D3D11 / D3D12
  spoutSenderNames m_dxSenderNames;

  // D3D11 backend
  spoutDirectX m_spoutDX;
  ID3D11Texture2D* m_sharedTexture{};
  HANDLE m_shareHandle{};

  // D3D12 backend (D3D11On12 interop)
  ID3D11On12Device* m_d3d11On12Device{};
  ID3D11Device* m_d3d11Device{};
  ID3D11DeviceContext* m_d3d11Context{};
  ID3D11Resource* m_wrappedTexture{};

  QRhi::Implementation m_backend{QRhi::OpenGLES2};
  bool m_created{};
};


SpoutDevice::~SpoutDevice() { }

void SpoutDevice::disconnect()
{
  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

bool SpoutDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto set = m_settings.deviceSpecificSettings.value<SharedOutputSettings>();
      m_protocol = new gfx_protocol_base{plug->exec};

      class spout_device : public ossia::net::device_base
      {
        gfx_node_base root;

      public:
        spout_device(
            const SharedOutputSettings& set,
            std::unique_ptr<gfx_protocol_base> proto, std::string name)
            : ossia::net::device_base{std::move(proto)}
            , root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), new SpoutNode{set}, name}
        {
        }

        const gfx_node_base& get_root_node() const override { return root; }
        gfx_node_base& get_root_node() override { return root; }
      };

      m_dev = std::make_unique<spout_device>(
          set, std::unique_ptr<gfx_protocol_base>(m_protocol),
          m_settings.name.toStdString());
      deviceChanged(nullptr, m_dev.get());
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
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

QString SpoutProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Spout Output");
}

QUrl SpoutProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/spout-device.html");
}

Device::DeviceInterface* SpoutProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& doc,
    const score::DocumentContext& ctx)
{
  return new SpoutDevice(settings, ctx);
}

const Device::DeviceSettings& SpoutProtocolFactory::defaultSettings() const noexcept
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
