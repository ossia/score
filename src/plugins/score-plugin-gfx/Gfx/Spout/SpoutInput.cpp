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
  // Vulkan-D3D11 interop using KMT handles (SpoutVK approach)
  // The Spout sender's shared texture is directly linked to a VkImage
  // using the legacy DXGI shared handle (KMT type)
  VkImage m_vkLinkedImage{};              // VkImage linked to Spout's shared D3D11 texture
  VkDeviceMemory m_vkLinkedMemory{};      // Device memory imported from D3D11 texture
  unsigned int m_vkSenderWidth{};
  unsigned int m_vkSenderHeight{};
  DWORD m_vkSenderFormat{};
  bool m_vkInitialized{};
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
    char senderName[256]{0};
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
    char senderName[256]{0};
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
    // Try to find and connect to the sender
    spoutSenderNames senderNames;
    char senderName[256]{0};
    strncpy_s(senderName, node.settings.path.toStdString().c_str(), 255);

    unsigned int senderWidth = 0, senderHeight = 0;
    DWORD dwFormat = 0;
    HANDLE shareHandle = nullptr;

    if(senderNames.GetSenderInfo(senderName, senderWidth, senderHeight, shareHandle, dwFormat))
    {
      w = senderWidth;
      h = senderHeight;
      m_sharedHandle = shareHandle;
      m_vkSenderWidth = senderWidth;
      m_vkSenderHeight = senderHeight;
      m_vkSenderFormat = dwFormat;
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
    char senderName[256]{0};
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
    char senderName[256]{0};
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
        return VK_FORMAT_R16G16B16A16_UNORM;
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      default:
        return VK_FORMAT_B8G8R8A8_UNORM;
    }
  }

  // Link a Vulkan image to D3D11 shared texture memory using KMT handle
  // Based on SpoutVK::LinkVulkanImage from the official SpoutVulkan examples
  bool linkVulkanImage(QRhi& rhi, HANDLE dxShareHandle, unsigned int w, unsigned int h, DWORD dwFormat)
  {
    if(m_vkInitialized)
      return false;

    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nativeHandles || !nativeHandles->dev || !nativeHandles->physDev)
      return false;

    VkDevice vkDevice = nativeHandles->dev;
    VkPhysicalDevice vkPhysDev = nativeHandles->physDev;

    VkFormat vulkanFormat = dxgiToVulkanFormat(dwFormat);

    // Release any previous resources
    releaseVulkanResources(rhi);

    // The handle type for Spout sender is KMT (legacy shared handle)
    // NOT NT handle - this is critical for Spout compatibility
    VkExternalMemoryHandleTypeFlags handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

    // Query support for external image format using KMT handles
    VkPhysicalDeviceImageFormatInfo2 formatInfo = {};
    formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    formatInfo.format = vulkanFormat;
    formatInfo.type = VK_IMAGE_TYPE_2D;
    formatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    formatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo = {};
    externalFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalFormatInfo.handleType = (VkExternalMemoryHandleTypeFlagBits)handleType;
    formatInfo.pNext = &externalFormatInfo;

    VkExternalImageFormatProperties externalImageFormatProps = {};
    externalImageFormatProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 imageFormatProps2 = {};
    imageFormatProps2.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    imageFormatProps2.pNext = &externalImageFormatProps;

    // Use vkGetPhysicalDeviceImageFormatProperties2 to check support
    auto* inst = score::gfx::staticVulkanInstance();
    if(!inst)
      return false;
    auto funcs = inst->functions();
    if(!funcs)
      return false;
    auto dfuncs = inst->deviceFunctions(vkDevice);
    if(!dfuncs)
      return false;

    // We need to use the device-level function for this
    auto vkGetPhysicalDeviceImageFormatProperties2Func
        = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            inst->getInstanceProcAddr("vkGetPhysicalDeviceImageFormatProperties2"));
    if(!vkGetPhysicalDeviceImageFormatProperties2Func)
      return false;

    VkResult result = vkGetPhysicalDeviceImageFormatProperties2Func(vkPhysDev, &formatInfo, &imageFormatProps2);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: KMT handle type not supported for Vulkan external memory";
      return false;
    }

    // Check if import is supported
    VkExternalMemoryFeatureFlags externalMemoryFeatures
        = externalImageFormatProps.externalMemoryProperties.externalMemoryFeatures;
    if(!(externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
    {
      qWarning() << "SpoutInput: Cannot import memory with KMT handle type";
      return false;
    }

    // Create the Vulkan import image with external memory info
    VkExternalMemoryImageCreateInfo extMemoryImageInfo = {};
    extMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extMemoryImageInfo.pNext = nullptr;
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
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = dfuncs->vkCreateImage(vkDevice, &imageCreateInfo, nullptr, &m_vkLinkedImage);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: Could not create Vulkan image for external memory";
      return false;
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    dfuncs->vkGetImageMemoryRequirements(vkDevice, m_vkLinkedImage, &memRequirements);

    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memProperties;
    funcs->vkGetPhysicalDeviceMemoryProperties(vkPhysDev, &memProperties);

    uint32_t memoryTypeIndex = UINT32_MAX;
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
      if((memRequirements.memoryTypeBits & (1 << i))
         && (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        memoryTypeIndex = i;
        break;
      }
    }

    if(memoryTypeIndex == UINT32_MAX)
    {
      qWarning() << "SpoutInput: No suitable memory type for external import";
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    // Set up import memory info with KMT handle
    VkImportMemoryWin32HandleInfoKHR importMemoryInfo = {};
    importMemoryInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importMemoryInfo.pNext = nullptr;
    importMemoryInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    importMemoryInfo.handle = dxShareHandle;
    importMemoryInfo.name = nullptr;

    // Check if dedicated allocation is required
    bool dedicatedRequired = (externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext = &importMemoryInfo;
    dedicatedAllocInfo.image = m_vkLinkedImage;
    dedicatedAllocInfo.buffer = VK_NULL_HANDLE;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = dedicatedRequired ? (void*)&dedicatedAllocInfo : (void*)&importMemoryInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    result = dfuncs->vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_vkLinkedMemory);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: Could not allocate memory for external import";
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    // Bind memory to the Vulkan image
    result = dfuncs->vkBindImageMemory(vkDevice, m_vkLinkedImage, m_vkLinkedMemory, 0);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutInput: Could not bind memory to image";
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
    // Check for sender updates
    spoutSenderNames senderNames;
    char senderName[256]{0};
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

    // Check if size, format, or handle changed
    bool needsRecreate = !m_vkInitialized
                         || senderWidth != m_vkSenderWidth
                         || senderHeight != m_vkSenderHeight
                         || dwFormat != m_vkSenderFormat
                         || shareHandle != m_sharedHandle;

    if(needsRecreate)
    {
      // Update stored values
      m_sharedHandle = shareHandle;
      m_vkSenderWidth = senderWidth;
      m_vkSenderHeight = senderHeight;
      m_vkSenderFormat = dwFormat;

      // Create linked Vulkan image from Spout's shared handle
      if(!linkVulkanImage(rhi, shareHandle, senderWidth, senderHeight, dwFormat))
      {
        enabled = false;
        return;
      }

      // Update metadata and texture size
      if(senderWidth != metadata.width || senderHeight != metadata.height)
      {
        metadata.width = senderWidth;
        metadata.height = senderHeight;
        material.scale[0] = 1.f;
        material.scale[1] = 1.f;
        material.textureSize[0] = metadata.width;
        material.textureSize[1] = metadata.height;
      }

      // Update QRhiTexture to use the linked VkImage
      SCORE_ASSERT(!m_gpu->samplers.empty());
      auto tex = m_gpu->samplers[0].texture;

      tex->destroy();
      tex->setPixelSize(QSize(senderWidth, senderHeight));

      QRhiTexture::NativeTexture nativeTex;
      nativeTex.object = (quint64)m_vkLinkedImage;
      // The linked image is in GENERAL layout for shared memory compatibility
      nativeTex.layout = VK_IMAGE_LAYOUT_GENERAL;

      if(!tex->createFrom(nativeTex))
      {
        qWarning() << "SpoutInput: Failed to create QRhiTexture from linked VkImage";
        releaseVulkanResources(rhi);
        enabled = false;
        return;
      }

      // Recreate shader resource bindings
      for(auto& pass : m_p)
        pass.second.srb->create();
    }

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

    if(m_vkLinkedMemory)
    {
      dfuncs->vkFreeMemory(vkDevice, m_vkLinkedMemory, nullptr);
      m_vkLinkedMemory = VK_NULL_HANDLE;
    }
    if(m_vkLinkedImage)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
    }
    m_vkInitialized = false;
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
        m_vkSenderWidth = 0;
        m_vkSenderHeight = 0;
        m_vkSenderFormat = 0;
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
