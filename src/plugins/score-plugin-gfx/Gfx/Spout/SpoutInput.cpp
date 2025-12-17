#include "SpoutInput.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>

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

// D3D11On12 for D3D12 interop
#include <d3d11on12.h>

// Vulkan interop
#if __has_include(<private/qrhivulkan_p.h>) && defined(QT_FEATURE_vulkan) && __has_include(<vulkan/vulkan.h>)
#define SCORE_SPOUT_VULKAN 1
#include <score/gfx/Vulkan.hpp>
#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <private/qrhivulkan_p.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan.hpp>
#include <dxgi1_2.h>
#endif

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

  // Spout receiver (for OpenGL)
  ::SpoutReceiver m_receiver;

  // Spout DirectX (for D3D11)
  spoutDirectX m_spoutDX;
  ID3D11Texture2D* m_receivedTexture{};
  HANDLE m_sharedHandle{};

  // D3D11On12 interop (for D3D12)
  ID3D11On12Device* m_d3d11On12Device{};
  ID3D11Device* m_d3d11Device{};
  ID3D11DeviceContext* m_d3d11Context{};
  ID3D11Resource* m_wrappedTexture{};
  ID3D11Texture2D* m_spoutSharedTexture{}; // Cached Spout shared texture

#if SCORE_SPOUT_VULKAN
  // Vulkan-D3D11 interop
  ID3D11Device* m_vkD3D11Device{};
  ID3D11DeviceContext* m_vkD3D11Context{};
  ID3D11Texture2D* m_vkSpoutTexture{};      // Spout's shared texture opened on our D3D11 device
  ID3D11Texture2D* m_vkInteropTexture{};    // Our texture with NT handle for Vulkan import
  HANDLE m_vkInteropHandle{};               // NT handle for Vulkan import
  VkImage m_vkExternalImage{};
  VkDeviceMemory m_vkExternalMemory{};
  QRhiTexture* m_vkRhiTexture{};            // QRhi texture wrapping the external VkImage
#endif

  bool enabled{};
  QRhi::Implementation m_backend{QRhi::Null};

  ~Renderer() { }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
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

    // Initialize based on backend
    unsigned int w = 0, h = 0;

    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        initOpenGL(rhi, w, h);
        break;
      case QRhi::D3D11:
        initD3D11(rhi, w, h);
        break;
      case QRhi::D3D12:
        initD3D12(rhi, w, h);
        break;
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        initVulkan(rhi, w, h);
        break;
#endif
      default:
        break;
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

    // Use BGRA for D3D/Vulkan backends (native DXGI format), RGBA for OpenGL
    auto format = (m_backend == QRhi::D3D11 || m_backend == QRhi::D3D12
                   || m_backend == QRhi::Vulkan)
                      ? QRhiTexture::BGRA8
                      : QRhiTexture::RGBA8;
    m_gpu = std::make_unique<score::gfx::PackedDecoder>(format, 4, metadata, QString{}, true);
    createPipelines(renderer);

    material.textureSize[0] = metadata.width;
    material.textureSize[1] = metadata.height;
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &material);
  }

  void initOpenGL(QRhi& rhi, unsigned int& w, unsigned int& h)
  {
    m_receiver.SetReceiverName(node.settings.path.toStdString().c_str());
    rhi.makeThreadLocalNativeContextCurrent();

    if(m_receiver.ReceiveTexture())
    {
      w = m_receiver.GetSenderWidth();
      h = m_receiver.GetSenderHeight();
      enabled = true;
    }
  }

  void initD3D11(QRhi& rhi, unsigned int& w, unsigned int& h)
  {
    // Get the D3D11 device from QRhi
    auto nativeHandles
        = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev)
      return;

    auto device = static_cast<ID3D11Device*>(nativeHandles->dev);

    // Initialize Spout DirectX with the QRhi device
    if(!m_spoutDX.OpenDirectX11(device))
      return;

    // Try to find and connect to the sender
    spoutSenderNames senderNames;
    char senderName[256];
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      w = senderWidth;
      h = senderHeight;
      m_sharedHandle = shareHandle;
      enabled = true;
    }
  }

  void initD3D12(QRhi& rhi, unsigned int& w, unsigned int& h)
  {
    // Get D3D12 device and command queue from QRhi
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

    // Get the D3D11On12Device interface
    hr = m_d3d11Device->QueryInterface(
        __uuidof(ID3D11On12Device), reinterpret_cast<void**>(&m_d3d11On12Device));
    if(FAILED(hr) || !m_d3d11On12Device)
    {
      m_d3d11Device->Release();
      m_d3d11Device = nullptr;
      m_d3d11Context->Release();
      m_d3d11Context = nullptr;
      return;
    }

    // Try to find and connect to the sender
    spoutSenderNames senderNames;
    char senderName[256];
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      w = senderWidth;
      h = senderHeight;
      m_sharedHandle = shareHandle;
      enabled = true;
    }
  }

#if SCORE_SPOUT_VULKAN
  void initVulkan(QRhi& rhi, unsigned int& w, unsigned int& h)
  {
    // Create a D3D11 device for Spout interop
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, 2,
        D3D11_SDK_VERSION, &m_vkD3D11Device, nullptr, &m_vkD3D11Context);

    if(FAILED(hr) || !m_vkD3D11Device)
      return;

    // Try to find and connect to the sender
    spoutSenderNames senderNames;
    char senderName[256];
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      w = senderWidth;
      h = senderHeight;
      m_sharedHandle = shareHandle;
      enabled = true;
    }
  }
#endif

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

    if(!m_receiver.ReceiveTexture())
    {
      enabled = false;
      return;
    }

    enabled = true;

    if(m_receiver.IsUpdated())
    {
      unsigned int w = m_receiver.GetSenderWidth();
      unsigned int h = m_receiver.GetSenderHeight();

      if(w > 0 && h > 0 && (w != metadata.width || h != metadata.height))
      {
        resizeTexture(tex, w, h);
        gltex->specified = true;
      }
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

    // Check for sender updates
    spoutSenderNames senderNames;
    char senderName[256];
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(!senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      enabled = false;
      return;
    }

    enabled = true;

    // Check if size or handle changed
    if(senderWidth != metadata.width || senderHeight != metadata.height
       || shareHandle != m_sharedHandle)
    {
      // Release cached shared texture if handle changed
      if(m_receivedTexture && shareHandle != m_sharedHandle)
      {
        m_receivedTexture->Release();
        m_receivedTexture = nullptr;
      }
      m_sharedHandle = shareHandle;
      resizeTexture(tex, senderWidth, senderHeight);
    }

    // Open the shared texture (cache it to avoid reopening every frame)
    if(!m_receivedTexture && m_sharedHandle)
    {
      HRESULT hr
          = device->OpenSharedResource(m_sharedHandle, IID_PPV_ARGS(&m_receivedTexture));
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

    // Check for sender updates
    spoutSenderNames senderNames;
    char senderName[256];
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(!senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      enabled = false;
      return;
    }

    enabled = true;

    // Check if size changed - need to re-wrap the texture
    bool sizeChanged = (senderWidth != metadata.width || senderHeight != metadata.height);
    bool handleChanged = (shareHandle != m_sharedHandle);

    if(sizeChanged || handleChanged)
    {
      // Release old wrapped resource
      if(m_wrappedTexture)
      {
        m_wrappedTexture->Release();
        m_wrappedTexture = nullptr;
      }

      m_sharedHandle = shareHandle;

      if(sizeChanged)
        resizeTexture(tex, senderWidth, senderHeight);
    }

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
    HRESULT hr
        = m_d3d11Device->OpenSharedResource(m_sharedHandle, IID_PPV_ARGS(&sharedTex));
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
  void updateVulkan(QRhi& rhi, QRhiResourceUpdateBatch& res)
  {
    if(!m_vkD3D11Device || !m_vkD3D11Context)
      return;

    // Check for sender updates
    spoutSenderNames senderNames;
    char senderName[256];
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(!senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      enabled = false;
      return;
    }

    enabled = true;

    // Check if size or handle changed
    bool sizeChanged = (senderWidth != metadata.width || senderHeight != metadata.height);
    bool handleChanged = (shareHandle != m_sharedHandle);

    if(sizeChanged || handleChanged)
    {
      // Need to recreate the Vulkan external image
      releaseVulkanResources(rhi);
      m_sharedHandle = shareHandle;

      if(sizeChanged)
      {
        metadata.width = senderWidth;
        metadata.height = senderHeight;
        material.textureSize[0] = metadata.width;
        material.textureSize[1] = metadata.height;
      }
    }

    // Open Spout's shared texture if not already open
    if(!m_vkSpoutTexture && m_sharedHandle)
    {
      HRESULT hr = m_vkD3D11Device->OpenSharedResource(
          m_sharedHandle, IID_PPV_ARGS(&m_vkSpoutTexture));
      if(FAILED(hr))
        m_vkSpoutTexture = nullptr;
    }

    if(!m_vkSpoutTexture)
      return;

    // Create the interop texture with NT handle if not already created
    if(!m_vkInteropTexture)
    {
      if(!createVulkanInteropTexture(rhi, senderWidth, senderHeight))
        return;
    }

    if(!m_vkInteropTexture || !m_vkExternalImage)
      return;

    // Copy from Spout texture to our interop texture
    m_vkD3D11Context->CopyResource(m_vkInteropTexture, m_vkSpoutTexture);
    m_vkD3D11Context->Flush();
  }

  bool createVulkanInteropTexture(QRhi& rhi, unsigned int w, unsigned int h)
  {
    // Create D3D11 texture with NT handle sharing for Vulkan import
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = w;
    texDesc.Height = h;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = m_vkD3D11Device->CreateTexture2D(&texDesc, nullptr, &m_vkInteropTexture);
    if(FAILED(hr) || !m_vkInteropTexture)
      return false;

    // Get the NT handle for Vulkan import
    IDXGIResource1* dxgiResource = nullptr;
    hr = m_vkInteropTexture->QueryInterface(__uuidof(IDXGIResource1), (void**)&dxgiResource);
    if(FAILED(hr) || !dxgiResource)
    {
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    hr = dxgiResource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
        &m_vkInteropHandle);
    dxgiResource->Release();

    if(FAILED(hr) || !m_vkInteropHandle)
    {
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    // Now import into Vulkan
    auto nativeHandles
        = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev || !nativeHandles->physDev)
      return false;

    VkDevice vkDevice = nativeHandles->dev;
    VkPhysicalDevice vkPhysDev = nativeHandles->physDev;

    // Create VkImage for external memory
    VkExternalMemoryImageCreateInfo extMemImageInfo = {};
    extMemImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extMemImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &extMemImageInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    imageInfo.extent = {w, h, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto* inst = score::gfx::staticVulkanInstance();
    SCORE_ASSERT(inst);
    auto funcs = inst->functions();
    SCORE_ASSERT(funcs);
    auto dfuncs = inst->deviceFunctions(vkDevice);
    SCORE_ASSERT(dfuncs);

    VkResult result = dfuncs->vkCreateImage(vkDevice, &imageInfo, nullptr, &m_vkExternalImage);
    if(result != VK_SUCCESS)
    {
      CloseHandle(m_vkInteropHandle);
      m_vkInteropHandle = nullptr;
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    // Get memory requirements
    VkMemoryRequirements memReqs;
    dfuncs->vkGetImageMemoryRequirements(vkDevice, m_vkExternalImage, &memReqs);

    // Import the D3D11 texture memory
    VkImportMemoryWin32HandleInfoKHR importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
    importInfo.handle = m_vkInteropHandle;

    VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = &importInfo;
    dedicatedInfo.image = m_vkExternalImage;

    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memProps;
    funcs->vkGetPhysicalDeviceMemoryProperties(vkPhysDev, &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    for(uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
      if((memReqs.memoryTypeBits & (1 << i))
         && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        memTypeIndex = i;
        break;
      }
    }

    if(memTypeIndex == UINT32_MAX)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkExternalImage, nullptr);
      m_vkExternalImage = VK_NULL_HANDLE;
      CloseHandle(m_vkInteropHandle);
      m_vkInteropHandle = nullptr;
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    result = dfuncs->vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_vkExternalMemory);
    if(result != VK_SUCCESS)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkExternalImage, nullptr);
      m_vkExternalImage = VK_NULL_HANDLE;
      CloseHandle(m_vkInteropHandle);
      m_vkInteropHandle = nullptr;
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    // Bind image to memory
    result = dfuncs->vkBindImageMemory(vkDevice, m_vkExternalImage, m_vkExternalMemory, 0);
    if(result != VK_SUCCESS)
    {
      dfuncs->vkFreeMemory(vkDevice, m_vkExternalMemory, nullptr);
      m_vkExternalMemory = VK_NULL_HANDLE;
      dfuncs->vkDestroyImage(vkDevice, m_vkExternalImage, nullptr);
      m_vkExternalImage = VK_NULL_HANDLE;
      CloseHandle(m_vkInteropHandle);
      m_vkInteropHandle = nullptr;
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    // Create QRhiTexture from the external VkImage
    SCORE_ASSERT(!m_gpu->samplers.empty());
    auto tex = m_gpu->samplers[0].texture;

    // Destroy old texture and recreate from external image
    tex->destroy();
    tex->setPixelSize(QSize(w, h));

    QRhiTexture::NativeTexture nativeTex;
    nativeTex.object = (quint64)m_vkExternalImage;
    nativeTex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if(!tex->createFrom(nativeTex))
    {
      dfuncs->vkFreeMemory(vkDevice, m_vkExternalMemory, nullptr);
      m_vkExternalMemory = VK_NULL_HANDLE;
      dfuncs->vkDestroyImage(vkDevice, m_vkExternalImage, nullptr);
      m_vkExternalImage = VK_NULL_HANDLE;
      CloseHandle(m_vkInteropHandle);
      m_vkInteropHandle = nullptr;
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
      return false;
    }

    m_vkRhiTexture = tex;

    // Recreate shader resource bindings
    for(auto& pass : m_p)
      pass.second.srb->create();

    return true;
  }

  void releaseVulkanResources(QRhi& rhi)
  {
    auto nativeHandles
        = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    VkDevice vkDevice = nativeHandles ? nativeHandles->dev : VK_NULL_HANDLE;

    auto* inst = score::gfx::staticVulkanInstance();
    SCORE_ASSERT(inst);
    auto funcs = inst->functions();
    SCORE_ASSERT(funcs);
    auto dfuncs = inst->deviceFunctions(vkDevice);
    SCORE_ASSERT(dfuncs);

    if(m_vkExternalMemory && vkDevice)
    {
      dfuncs->vkFreeMemory(vkDevice, m_vkExternalMemory, nullptr);
      m_vkExternalMemory = VK_NULL_HANDLE;
    }
    if(m_vkExternalImage && vkDevice)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkExternalImage, nullptr);
      m_vkExternalImage = VK_NULL_HANDLE;
    }
    if(m_vkInteropHandle)
    {
      CloseHandle(m_vkInteropHandle);
      m_vkInteropHandle = nullptr;
    }
    if(m_vkInteropTexture)
    {
      m_vkInteropTexture->Release();
      m_vkInteropTexture = nullptr;
    }
    if(m_vkSpoutTexture)
    {
      m_vkSpoutTexture->Release();
      m_vkSpoutTexture = nullptr;
    }
    m_vkRhiTexture = nullptr;
  }
#endif

  void resizeTexture(QRhiTexture* tex, unsigned int w, unsigned int h)
  {
    metadata.width = w;
    metadata.height = h;
    material.scale[0] = 1.f;
    material.scale[1] = 1.f;
    material.textureSize[0] = metadata.width;
    material.textureSize[1] = metadata.height;

    tex->destroy();
    tex->setPixelSize(QSize(w, h));
    tex->create();

    for(auto& pass : m_p)
      pass.second.srb->create();
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
    switch(m_backend)
    {
      case QRhi::OpenGLES2:
        if(enabled)
          m_receiver.ReleaseReceiver();
        break;
      case QRhi::D3D11:
        if(m_receivedTexture)
        {
          m_receivedTexture->Release();
          m_receivedTexture = nullptr;
        }
        break;
      case QRhi::D3D12:
        // Release D3D11On12 resources
        if(m_wrappedTexture)
        {
          m_wrappedTexture->Release();
          m_wrappedTexture = nullptr;
        }
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
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        releaseVulkanResources(*r.state.rhi);
        if(m_vkD3D11Context)
        {
          m_vkD3D11Context->Release();
          m_vkD3D11Context = nullptr;
        }
        if(m_vkD3D11Device)
        {
          m_vkD3D11Device->Release();
          m_vkD3D11Device = nullptr;
        }
        break;
#endif
      default:
        break;
    }

    enabled = false;
    m_receivedTexture = nullptr;
    m_sharedHandle = nullptr;

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
