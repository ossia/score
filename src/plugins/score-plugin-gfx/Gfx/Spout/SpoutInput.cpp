#include "SpoutInput.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>

#include <ossia/detail/algorithms.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <rhi/qrhi.h>

#include <QFormLayout>
#include <QLabel>
#include <QUrl>

#include <Spout/SpoutReceiver.h>
#include <Spout/SpoutDirectX.h>

// Include QRhi D3D11/D3D12 private headers for native texture access
#include <private/qrhid3d11_p.h>
#include <private/qrhid3d12_p.h>

// clang-format off
// D3D11On12 for D3D12 interop
#include <windows.h>
#include <d3d11on12.h>
// clang-format on

// Vulkan interop
#if __has_include(<private/qrhivulkan_p.h>) && defined(QT_FEATURE_vulkan) && __has_include(<vulkan/vulkan.h>)
#define SCORE_SPOUT_VULKAN 1
#include <score/gfx/Vulkan.hpp>
#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <private/qrhivulkan_p.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#endif

#include <wobjectimpl.h>

#include <set>

namespace Gfx::Spout
{
namespace
{
// Cached snapshot of what we last observed from the Spout sender.
// Allows detecting size/format/handle changes between frames.
struct SpoutSenderInfo
{
  unsigned int width{};
  unsigned int height{};
  DWORD dxgiFormat{};
  HANDLE handle{};

  friend bool operator==(const SpoutSenderInfo&, const SpoutSenderInfo&) noexcept
      = default;
};

bool querySpoutSender(const char* name, SpoutSenderInfo& out) noexcept
{
  spoutSenderNames senders;
  return senders.GetSenderInfo(name, out.width, out.height, out.handle, out.dxgiFormat);
}

QRhiTexture::Format
dxgiToQRhiFormat(DWORD dxgi, QRhi::Implementation backend) noexcept
{
  // For OpenGL we keep RGBA channel order regardless of sender layout:
  // Spout's GL-DX interop handles the BGRA<->RGBA conversion on its side.
  const bool wantNativeBGRA = (backend == QRhi::D3D11 || backend == QRhi::D3D12
                               || backend == QRhi::Vulkan);

  switch(static_cast<DXGI_FORMAT>(dxgi))
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      return QRhiTexture::RGBA8;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
      return wantNativeBGRA ? QRhiTexture::BGRA8 : QRhiTexture::RGBA8;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      return QRhiTexture::RGB10A2;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      // RGBA16F is the only 4x16 format QRhi exposes (no RGBA16-UNORM). For a
      // _UNORM sender this samples as half-float (color-inaccurate) but is the
      // only available 64-bit/pixel format; dxgiToVulkanFormat() maps the same
      // DXGI formats to VK_FORMAT_R16G16B16A16_SFLOAT so the imported VkImage
      // and the QRhi-created view agree (no validation violation). On D3D the
      // CopyResource between _UNORM and _FLOAT is permitted (shared TYPELESS
      // family) and bit-preserving.
      return QRhiTexture::RGBA16F;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      return QRhiTexture::RGBA32F;
    default:
      return wantNativeBGRA ? QRhiTexture::BGRA8 : QRhiTexture::RGBA8;
  }
}
}

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
  std::pair<QShader, QShader> m_shaders;

  // Spout receiver (for OpenGL)
  ::SpoutReceiver m_receiver;

  // Spout DirectX (for D3D11)
  spoutDirectX m_spoutDX;
  ID3D11Texture2D* m_receivedTexture{};

  // D3D11On12 interop (for D3D12)
  ID3D11On12Device* m_d3d11On12Device{};
  ID3D11Device* m_d3d11Device{};
  ID3D11DeviceContext* m_d3d11Context{};
  ID3D11Resource* m_wrappedTexture{};

#if SCORE_SPOUT_VULKAN
  // Vulkan-D3D11 interop using KMT handles (SpoutVK approach)
  // The Spout sender's shared texture is directly linked to a VkImage
  // using the legacy DXGI shared handle (KMT type)
  VkImage m_vkLinkedImage{};              // VkImage linked to Spout's shared D3D11 texture
  VkDeviceMemory m_vkLinkedMemory{};      // Device memory imported from D3D11 texture
  bool m_vkInitialized{};
#endif

  // Last-known sender info — used to detect size/format/handle changes.
  SpoutSenderInfo m_lastSender{};
  // Current destination texture format (may differ from sender DXGI byte-order on OpenGL).
  QRhiTexture::Format m_textureFormat{QRhiTexture::RGBA8};

  bool enabled{};
  QRhi::Implementation m_backend{QRhi::Null};

  ~Renderer() { }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    m_backend = rhi.backend();

    // Initialize our rendering structures
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

    // Backend-specific bring-up (creates D3D11On12 device, OpenGL receiver context, etc.)
    // Does NOT allocate the destination texture — that happens once we know the format.
    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        initOpenGL(rhi);
        break;
      case QRhi::D3D11:
        initD3D11(rhi);
        break;
      case QRhi::D3D12:
        initD3D12(rhi);
        break;
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        initVulkan(rhi);
        break;
#endif
      default:
        break;
    }

    // Probe sender once up-front so we can pick a matching texture format.
    // If no sender is present yet, fall through to safe defaults and let the
    // first successful update() reconfigure to the real format.
    SpoutSenderInfo si;
    if(querySpoutSender(node.settings.path.toStdString().c_str(), si)
       && si.width > 0 && si.height > 0)
    {
      enabled = true;
    }
    else
    {
      si = {};
      si.width = 1280;
      si.height = 720;
      // Default DXGI format mirrors the previous fallback (BGRA on D3D/Vulkan, RGBA on GL)
      si.dxgiFormat = (m_backend == QRhi::D3D11 || m_backend == QRhi::D3D12
                       || m_backend == QRhi::Vulkan)
                          ? DXGI_FORMAT_B8G8R8A8_UNORM
                          : DXGI_FORMAT_R8G8B8A8_UNORM;
      enabled = false;
    }

    m_lastSender = si;
    m_textureFormat = dxgiToQRhiFormat(si.dxgiFormat, m_backend);
    metadata.width = si.width;
    metadata.height = si.height;

    m_gpu = std::make_unique<score::gfx::PackedDecoder>(
        m_textureFormat, 4, metadata, QString{}, true);

    // Cache shaders from GPU decoder init
    if(m_gpu)
      m_shaders = m_gpu->init(renderer);

    material.textureSize[0] = metadata.width;
    material.textureSize[1] = metadata.height;
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);

    m_initialized = true;
  }

  void addOutputPass(
      score::gfx::RenderList& renderer, score::gfx::Edge& edge,
      QRhiResourceUpdateBatch& res) override
  {
    if(!m_gpu)
      return;
    if(!m_shaders.first.isValid() || !m_shaders.second.isValid())
      return;

    auto rt = renderer.renderTargetForOutput(edge);
    if(rt.renderTarget)
    {
      auto pip = score::gfx::buildPipeline(
          renderer, renderer.defaultTriangle(), m_shaders.first, m_shaders.second, rt,
          m_processUBO, m_materialUBO, m_gpu->samplers);
      if(pip.pipeline)
        m_p.emplace_back(&edge, score::gfx::Pass{rt, pip, nullptr});
    }
  }

  void removeOutputPass(score::gfx::RenderList& renderer, score::gfx::Edge& edge) override
  {
    auto it = ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; });
    if(it != m_p.end())
    {
      it->second.release();
      m_p.erase(it);
    }
  }

  bool hasOutputPassForEdge(score::gfx::Edge& edge) const override
  {
    return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
           != m_p.end();
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);

    for(auto* edge : this->node.output[0]->edges)
      addOutputPass(renderer, *edge, res);
  }

  void initOpenGL(QRhi& rhi)
  {
    m_receiver.SetReceiverName(node.settings.path.toStdString().c_str());
    rhi.makeThreadLocalNativeContextCurrent();
  }

  void initD3D11(QRhi& rhi)
  {
    auto nativeHandles
        = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev)
      return;

    auto device = static_cast<ID3D11Device*>(nativeHandles->dev);
    m_spoutDX.OpenDirectX11(device);
  }

  void initD3D12(QRhi& rhi)
  {
    auto nativeHandles
        = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev || !nativeHandles->commandQueue)
      return;

    auto d3d12Device = static_cast<ID3D12Device*>(nativeHandles->dev);
    IUnknown* cmdQueue = static_cast<IUnknown*>(nativeHandles->commandQueue);

    // Create D3D11On12 device for interop
    UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11On12CreateDevice(
        d3d12Device, d3d11DeviceFlags, nullptr, 0, &cmdQueue, 1, 0, &m_d3d11Device,
        &m_d3d11Context, nullptr);

    if(FAILED(hr) || !m_d3d11Device)
      return;

    hr = m_d3d11Device->QueryInterface(
        __uuidof(ID3D11On12Device), reinterpret_cast<void**>(&m_d3d11On12Device));
    if(FAILED(hr) || !m_d3d11On12Device)
    {
      m_d3d11Device->Release();
      m_d3d11Device = nullptr;
      m_d3d11Context->Release();
      m_d3d11Context = nullptr;
    }
  }

#if SCORE_SPOUT_VULKAN
  void initVulkan(QRhi& /*rhi*/) { }
#endif


  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    auto& rhi = *renderer.state.rhi;

    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        updateOpenGL(rhi, res);
        break;
      case QRhi::D3D11:
        updateD3D11(rhi, res);
        break;
      case QRhi::D3D12:
        updateD3D12(rhi, res);
        break;
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        updateVulkan(rhi, res);
        break;
#endif
      default:
        break;
    }

    res.updateDynamicBuffer(
        m_processUBO, 0, sizeof(score::gfx::ProcessUBO), &this->node.standardUBO);
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);
  }

  void updateOpenGL(QRhi& rhi, QRhiResourceUpdateBatch& res)
  {
    rhi.makeThreadLocalNativeContextCurrent();

    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;
    auto gltex = static_cast<QGles2Texture*>(tex);

    // Probe sender presence — this also lets Spout update its internal
    // m_bUpdated flag, which IsUpdated() then reports/clears.
    if(!m_receiver.ReceiveTexture())
    {
      enabled = false;
      return;
    }

    enabled = true;

    // Pull the full sender state (size + DXGI format + handle) for change detection.
    // GetSenderInfo reads from the Spout sender-names shared memory and is cheap.
    SpoutSenderInfo si;
    if(querySpoutSender(node.settings.path.toStdString().c_str(), si)
       && si.width > 0 && si.height > 0)
    {
      if(reconfigureIfNeeded(rhi, si))
        gltex->specified = true;
    }

    GLuint texId = gltex->texture;
    m_receiver.ReceiveTexture(texId, GL_TEXTURE_2D);
  }

  void updateD3D11(QRhi& rhi, QRhiResourceUpdateBatch& res)
  {
    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;
    auto d3dtex = static_cast<QD3D11Texture*>(tex);

    // Get QRhi's D3D11 context - we must use the same context for the copy to be visible
    auto nativeHandles
        = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev || !nativeHandles->context)
      return;

    auto device = static_cast<ID3D11Device*>(nativeHandles->dev);
    auto context = static_cast<ID3D11DeviceContext*>(nativeHandles->context);

    SpoutSenderInfo si;
    if(!querySpoutSender(node.settings.path.toStdString().c_str(), si))
    {
      enabled = false;
      return;
    }

    enabled = true;

    // Recreate the destination texture if anything changed.
    // Important: D3D11 CopyResource requires source & destination formats to match,
    // so we have to honor the sender's DXGI format here.
    reconfigureIfNeeded(rhi, si);

    // Open the shared texture (cache it to avoid reopening every frame)
    if(!m_receivedTexture && m_lastSender.handle)
    {
      HRESULT hr = device->OpenSharedResource(
          m_lastSender.handle, IID_PPV_ARGS(&m_receivedTexture));
      if(FAILED(hr))
        m_receivedTexture = nullptr;
    }

    if(m_receivedTexture && d3dtex->tex)
    {
      // Copy from shared texture to our texture using QRhi's context
      context->CopyResource(d3dtex->tex, m_receivedTexture);
    }
  }

  void updateD3D12(QRhi& rhi, QRhiResourceUpdateBatch& res)
  {
    if(!m_d3d11On12Device || !m_d3d11Device || !m_d3d11Context)
      return;

    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;

    SpoutSenderInfo si;
    if(!querySpoutSender(node.settings.path.toStdString().c_str(), si))
    {
      enabled = false;
      return;
    }

    enabled = true;

    // Recreate destination texture (and drop the cached D3D11 wrapped resource)
    // when the sender's size, format or share handle changes.
    reconfigureIfNeeded(rhi, si);

    // Get the native D3D12 resource from QRhiTexture
    auto nativeTex = tex->nativeTexture();
    auto d3d12Resource = reinterpret_cast<ID3D12Resource*>(nativeTex.object);
    if(!d3d12Resource)
      return;

    // Wrap the D3D12 texture for D3D11 access if not already wrapped
    if(!m_wrappedTexture)
    {
      D3D11_RESOURCE_FLAGS d3d11Flags = {};
      d3d11Flags.BindFlags = D3D11_BIND_SHADER_RESOURCE;

      HRESULT hr = m_d3d11On12Device->CreateWrappedResource(
          d3d12Resource, &d3d11Flags, D3D12_RESOURCE_STATE_COPY_DEST,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, IID_PPV_ARGS(&m_wrappedTexture));

      if(FAILED(hr))
      {
        m_wrappedTexture = nullptr;
        return;
      }
    }

    if(!m_wrappedTexture)
      return;

    // Open the Spout shared texture via D3D11
    ID3D11Texture2D* sharedTex = nullptr;
    HRESULT hr = m_d3d11Device->OpenSharedResource(
        m_lastSender.handle, IID_PPV_ARGS(&sharedTex));
    if(FAILED(hr) || !sharedTex)
      return;

    // Acquire the wrapped resource for D3D11 use
    m_d3d11On12Device->AcquireWrappedResources(&m_wrappedTexture, 1);

    // Copy from shared texture to wrapped texture
    m_d3d11Context->CopyResource(m_wrappedTexture, sharedTex);

    // Release the wrapped resource back to D3D12
    m_d3d11On12Device->ReleaseWrappedResources(&m_wrappedTexture, 1);

    // Flush D3D11 commands
    m_d3d11Context->Flush();

    // Release the shared texture reference
    sharedTex->Release();
  }

#if SCORE_SPOUT_VULKAN
  // Convert DXGI format to Vulkan format (from SpoutVK)
  static VkFormat dxgiToVulkanFormat(DWORD dwFormat)
  {
    switch((DXGI_FORMAT)dwFormat)
    {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      case DXGI_FORMAT_R16G16B16A16_UNORM:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        // The QRhi destination texture for both of these is RGBA16F (the only
        // 4x16 format QRhi exposes — there is no RGBA16-UNORM). The imported
        // VkImage MUST use the same format as the QRhi-created image view,
        // otherwise QVkTexture::createFrom() builds an SFLOAT view over a
        // non-MUTABLE_FORMAT UNORM image, which is a Vulkan validation
        // violation (VUID-VkImageViewCreateInfo-image-01762) and samples
        // garbage. Both _UNORM and _FLOAT are 64-bit/pixel, so the KMT import
        // succeeds; we therefore map both to SFLOAT to stay consistent with
        // dxgiToQRhiFormat(). (UNORM data read as half-float is still color-
        // inaccurate, but that is an inherent QRhi limitation, not a crash.)
        return VK_FORMAT_R16G16B16A16_SFLOAT;
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      default:
        return VK_FORMAT_B8G8R8A8_UNORM;
    }
  }

  // Link a Vulkan image to D3D11 shared texture memory using KMT handle.
  // Caller is expected to have torn down any prior linked resources via
  // releaseVulkanResources() and the QRhiTexture's destroy() before calling.
  bool linkVulkanImage(QRhi& rhi, HANDLE dxShareHandle, unsigned int w, unsigned int h, DWORD dwFormat)
  {
    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev || !nativeHandles->physDev)
      return false;

    VkDevice vkDevice = nativeHandles->dev;
    VkPhysicalDevice vkPhysDev = nativeHandles->physDev;

    VkFormat vulkanFormat = dxgiToVulkanFormat(dwFormat);

    // Defensive: ensure nothing leaks if caller did not release first.
    releaseVulkanResources(rhi);

    // Spout shares D3D11 textures via legacy KMT handles (NOT NT handles).
    constexpr auto handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

    auto* inst = score::gfx::staticVulkanInstance();
    if(!inst)
      return false;
    auto funcs = inst->functions();
    if(!funcs)
      return false;
    auto dfuncs = inst->deviceFunctions(vkDevice);
    if(!dfuncs)
      return false;

    // Resolve vkGetMemoryWin32HandlePropertiesKHR via vkGetDeviceProcAddr.
    //
    // Why not inst->getInstanceProcAddr("vkGetMemoryWin32HandlePropertiesKHR")?
    // Qt forwards that to vkGetInstanceProcAddr, which for device-level
    // extension functions can return a non-null trampoline that CRASHES
    // when called: the instance loader has no per-device dispatch for
    // device extensions, so calling that pointer dereferences garbage.
    //
    // vkGetDeviceProcAddr is itself a core 1.0 function, so resolving IT
    // through inst->getInstanceProcAddr is safe — that part of the loader
    // has proper dispatch. We then call the device-level resolver to get
    // a pointer that's valid for THIS device's enabled extensions.
    PFN_vkGetMemoryWin32HandlePropertiesKHR pfnGetMemWin32Props = nullptr;
    {
      auto pfnGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
          inst->getInstanceProcAddr("vkGetDeviceProcAddr"));
      if(pfnGetDeviceProcAddr)
      {
        pfnGetMemWin32Props
            = reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
                pfnGetDeviceProcAddr(
                    vkDevice, "vkGetMemoryWin32HandlePropertiesKHR"));
      }
    }

    // Probe whether import for this format/handle type is supported.
    // Note: this is informational; the real test is the memory-type
    // intersection below.
    VkExternalMemoryFeatureFlags externalMemoryFeatures = 0;
    {
      VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo = {};
      externalFormatInfo.sType
          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
      externalFormatInfo.handleType = handleType;

      VkPhysicalDeviceImageFormatInfo2 formatInfo = {};
      formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
      formatInfo.pNext = &externalFormatInfo;
      formatInfo.format = vulkanFormat;
      formatInfo.type = VK_IMAGE_TYPE_2D;
      formatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      formatInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

      VkExternalImageFormatProperties extProps = {};
      extProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;

      VkImageFormatProperties2 props2 = {};
      props2.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
      props2.pNext = &extProps;

      auto pfnGetPhysFmt2 = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
          inst->getInstanceProcAddr("vkGetPhysicalDeviceImageFormatProperties2"));
      if(pfnGetPhysFmt2)
      {
        VkResult r = pfnGetPhysFmt2(vkPhysDev, &formatInfo, &props2);
        if(r == VK_SUCCESS)
          externalMemoryFeatures = extProps.externalMemoryProperties.externalMemoryFeatures;
      }
    }

    // Create the VkImage with external memory info.
    VkExternalMemoryImageCreateInfo extMemoryImageInfo = {};
    extMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extMemoryImageInfo.handleTypes = handleType;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = &extMemoryImageInfo;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = vulkanFormat;
    imageCreateInfo.extent = {w, h, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result
        = dfuncs->vkCreateImage(vkDevice, &imageCreateInfo, nullptr, &m_vkLinkedImage);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: vkCreateImage failed for external memory:" << result;
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    // Memory requirements as dictated by the image we just created.
    VkMemoryRequirements memRequirements;
    dfuncs->vkGetImageMemoryRequirements(vkDevice, m_vkLinkedImage, &memRequirements);

    // For an imported KMT handle, the spec requires picking a memoryTypeIndex
    // from the intersection of memRequirements.memoryTypeBits and the bits
    // returned by vkGetMemoryWin32HandlePropertiesKHR for that handle.
    uint32_t handleMemoryTypeBits = 0;
    if(pfnGetMemWin32Props)
    {
      VkMemoryWin32HandlePropertiesKHR handleProps = {};
      handleProps.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
      VkResult hr
          = pfnGetMemWin32Props(vkDevice, handleType, dxShareHandle, &handleProps);
      if(hr == VK_SUCCESS)
        handleMemoryTypeBits = handleProps.memoryTypeBits;
      else
        qWarning() << "SpoutInput: vkGetMemoryWin32HandlePropertiesKHR failed:" << hr;
    }
    else
    {
      qWarning() << "SpoutInput: vkGetMemoryWin32HandlePropertiesKHR not available";
    }

    const uint32_t supportedBits
        = memRequirements.memoryTypeBits & handleMemoryTypeBits;
    if(supportedBits == 0)
    {
      qWarning() << "SpoutInput: No memory type supports the shared KMT handle"
                 << "(memReqBits=" << Qt::hex << memRequirements.memoryTypeBits
                 << "handleBits=" << handleMemoryTypeBits << ")";
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    VkPhysicalDeviceMemoryProperties memProperties;
    funcs->vkGetPhysicalDeviceMemoryProperties(vkPhysDev, &memProperties);

    // Prefer DEVICE_LOCAL among compatible types; fall back to any compatible.
    uint32_t memoryTypeIndex = UINT32_MAX;
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
      if((supportedBits & (1u << i))
         && (memProperties.memoryTypes[i].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        memoryTypeIndex = i;
        break;
      }
    }
    if(memoryTypeIndex == UINT32_MAX)
    {
      for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
      {
        if(supportedBits & (1u << i))
        {
          memoryTypeIndex = i;
          break;
        }
      }
    }
    if(memoryTypeIndex == UINT32_MAX)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    // Import the KMT handle.
    VkImportMemoryWin32HandleInfoKHR importMemoryInfo = {};
    importMemoryInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importMemoryInfo.handleType = handleType;
    importMemoryInfo.handle = dxShareHandle;

    // Dedicated allocation: KMT-imported memory backs exactly one image,
    // so we always dedicate. Required by some drivers, harmless on others.
    (void)externalMemoryFeatures;
    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext = &importMemoryInfo;
    dedicatedAllocInfo.image = m_vkLinkedImage;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedAllocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    result = dfuncs->vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_vkLinkedMemory);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: vkAllocateMemory for external import failed:" << result;
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      m_vkLinkedMemory = VK_NULL_HANDLE;
      return false;
    }

    result = dfuncs->vkBindImageMemory(vkDevice, m_vkLinkedImage, m_vkLinkedMemory, 0);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: vkBindImageMemory failed:" << result;
      dfuncs->vkFreeMemory(vkDevice, m_vkLinkedMemory, nullptr);
      m_vkLinkedMemory = VK_NULL_HANDLE;
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    m_vkInitialized = true;
    return true;
  }

  void updateVulkan(QRhi& rhi, QRhiResourceUpdateBatch& res)
  {
    SpoutSenderInfo si;
    if(!querySpoutSender(node.settings.path.toStdString().c_str(), si))
    {
      enabled = false;
      return;
    }

    enabled = true;

    // On Vulkan the destination QRhiTexture must be (re)linked to the
    // sender's shared D3D11 memory whenever size, format or handle changes.
    // The first frame after init also flows through here because m_vkInitialized
    // is still false (initState only allocates a plain placeholder texture).
    if(!m_vkInitialized)
    {
      // Force reconfiguration even if state happens to match the placeholder.
      m_lastSender = {};
    }
    reconfigureIfNeeded(rhi, si);

    // The texture content is automatically synchronized because
    // the VkImage memory is linked to the D3D11 shared texture.
    // When the sender updates the D3D11 texture, our VkImage sees the changes.
  }

  void releaseVulkanResources(QRhi& rhi)
  {
    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev)
    {
      m_vkLinkedImage = VK_NULL_HANDLE;
      m_vkLinkedMemory = VK_NULL_HANDLE;
      m_vkInitialized = false;
      return;
    }

    VkDevice vkDevice = nativeHandles->dev;

    auto* inst = score::gfx::staticVulkanInstance();
    if(!inst)
      return;
    auto dfuncs = inst->deviceFunctions(vkDevice);
    if(!dfuncs)
      return;

    // Destroy the image (and any binding to memory) before freeing the memory.
    if(m_vkLinkedImage)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
    }
    if(m_vkLinkedMemory)
    {
      dfuncs->vkFreeMemory(vkDevice, m_vkLinkedMemory, nullptr);
      m_vkLinkedMemory = VK_NULL_HANDLE;
    }
    m_vkInitialized = false;
  }
#endif

  // Drop backend-specific caches that are tied to the previous sender handle,
  // format or size. Called from reconfigureIfNeeded() before recreating the
  // destination texture, and from releaseState() during teardown.
  void releaseSharedResources(QRhi& rhi)
  {
    switch(m_backend)
    {
      case QRhi::D3D11:
        if(m_receivedTexture)
        {
          m_receivedTexture->Release();
          m_receivedTexture = nullptr;
        }
        break;
      case QRhi::D3D12:
        if(m_wrappedTexture)
        {
          m_wrappedTexture->Release();
          m_wrappedTexture = nullptr;
        }
        break;
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        releaseVulkanResources(rhi);
        break;
#endif
      default:
        break;
    }
  }

  // Returns true if anything was reconfigured (texture recreated). When that
  // happens, callers may need to refresh backend-specific state that depends
  // on the underlying QRhiTexture (e.g. OpenGL's `specified` flag).
  //
  // Always ensures the QRhiTexture has a valid backing on return (either a
  // linked import or a plain placeholder), so the SRB rebuild that follows
  // never produces a null VkImageView descriptor write.
  bool reconfigureIfNeeded(QRhi& rhi, const SpoutSenderInfo& sender)
  {
    if(sender.width == 0 || sender.height == 0)
      return false;

    const QRhiTexture::Format newFormat
        = dxgiToQRhiFormat(sender.dxgiFormat, m_backend);

    const bool sizeChanged
        = sender.width != m_lastSender.width || sender.height != m_lastSender.height;
    const bool formatChanged = newFormat != m_textureFormat;
    const bool handleChanged = sender.handle != m_lastSender.handle;
    if(!sizeChanged && !formatChanged && !handleChanged)
      return false;

    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;

    // Tear-down order matters: the QRhi-owned VkImageView (or D3D SRV) must
    // be destroyed BEFORE the underlying native resource it was created
    // from. Calling tex->destroy() first does the former; then
    // releaseSharedResources() drops the latter.
    tex->destroy();
    releaseSharedResources(rhi);

    tex->setPixelSize(QSize(sender.width, sender.height));
    tex->setFormat(newFormat);

    bool linked = false;
#if SCORE_SPOUT_VULKAN
    if(m_backend == QRhi::Vulkan)
    {
      if(linkVulkanImage(
             rhi, sender.handle, sender.width, sender.height, sender.dxgiFormat))
      {
        QRhiTexture::NativeTexture nt;
        nt.object = (quint64)m_vkLinkedImage;
        nt.layout = VK_IMAGE_LAYOUT_GENERAL;
        if(tex->createFrom(nt))
        {
          linked = true;
        }
        else
        {
          qWarning() << "SpoutInput: createFrom(VkImage) failed during reconfigure";
          releaseVulkanResources(rhi);
        }
      }
    }
#endif

    bool ok = linked;
    if(!ok)
    {
      // Either non-Vulkan path, or Vulkan link failed. Allocate a normal
      // QRhiTexture so the SRB has a valid view to bind. On Vulkan this
      // yields a black/undefined image but avoids the
      // VUID-VkWriteDescriptorSet-descriptorType-02997 validation error
      // and the subsequent draw-time crash.
      ok = tex->create();
    }

    if(!ok)
    {
      enabled = false;
      // Do NOT advance m_lastSender — let the next frame retry from scratch.
      return false;
    }

    // Update metadata + material UBO.
    metadata.width = sender.width;
    metadata.height = sender.height;
    material.scale[0] = 1.f;
    material.scale[1] = 1.f;
    material.textureSize[0] = metadata.width;
    material.textureSize[1] = metadata.height;

    m_textureFormat = newFormat;
    m_lastSender = sender;
#if SCORE_SPOUT_VULKAN
    if(m_backend == QRhi::Vulkan && !linked)
    {
      // Link failed for this sender configuration. We mark the renderer as
      // disabled (so callers can show a fallback frame) but record the
      // sender state so we don't churn through destroy/create every frame.
      // A natural retry happens when the sender's size, format or share
      // handle changes.
      enabled = false;
    }
#endif

    // Pipelines stay valid (only the input sampler binding changed), but the
    // SRB references the QRhiTexture pointer/format and must be rebuilt.
    for(auto& pass : m_p)
      pass.second.p.srb->create();

    return true;
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultTriangle();
    score::gfx::defaultRenderPass(renderer, mesh, m_meshBuffer, cb, edge, m_p);
  }

  void releaseState(score::gfx::RenderList& r) override
  {
    if(!m_initialized)
      return;

    // Order matters: destroy QRhi-owned resources (QRhiTexture wrappers and
    // their image views) BEFORE the underlying native shared resources they
    // wrap. Otherwise the QRhiTexture destruction may operate on a view
    // whose underlying VkImage / D3D resource has already been released.
    if(m_gpu)
    {
      m_gpu->release(r);
    }

    // Now drop the native shared resources we hold.
    releaseSharedResources(*r.state.rhi);

    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        if(enabled)
          m_receiver.ReleaseReceiver();
        break;
      case QRhi::D3D12:
        // Release the D3D11On12 interop layer (set up in initD3D12).
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
      default:
        break;
    }

    enabled = false;
    m_lastSender = {};
    m_textureFormat = QRhiTexture::RGBA8;

    delete m_processUBO;
    m_processUBO = nullptr;
    delete m_materialUBO;
    m_materialUBO = nullptr;

    for(auto& p : m_p)
      p.second.release();
    m_p.clear();

    m_meshBuffer = {};
    m_shaders = {};

    m_initialized = false;
  }

  void release(score::gfx::RenderList& r) override
  {
    releaseState(r);
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
