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

// Vulkan interop
#if __has_include(<private/qrhivulkan_p.h>) && defined(QT_FEATURE_vulkan) && __has_include(<vulkan/vulkan.h>)
#define SCORE_SPOUT_VULKAN 1
#include <score/gfx/Vulkan.hpp>
#include <Gfx/Settings/Model.hpp>
#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <private/qrhivulkan_p.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#endif

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
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        startRenderingVulkan();
        break;
#endif
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

#if SCORE_SPOUT_VULKAN
  void startRenderingVulkan()
  {
    if(!m_vkD3D11Device || !m_vkD3D11Context)
      return;

    if(!m_renderState || !m_renderState->rhi)
      return;

    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(
        m_renderState->rhi->nativeHandles());
    if(!nativeHandles || !nativeHandles->dev || !nativeHandles->physDev)
      return;

    VkDevice vkDevice = nativeHandles->dev;
    VkPhysicalDevice vkPhysDev = nativeHandles->physDev;

    // Create shared D3D11 texture for Spout
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_settings.width;
    texDesc.Height = m_settings.height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = m_vkD3D11Device->CreateTexture2D(&texDesc, nullptr, &m_vkSharedTexture);
    if(FAILED(hr) || !m_vkSharedTexture)
      return;

    // Get the shared handle
    IDXGIResource* dxgiResource = nullptr;
    hr = m_vkSharedTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
    if(SUCCEEDED(hr) && dxgiResource)
    {
      dxgiResource->GetSharedHandle(&m_vkShareHandle);
      dxgiResource->Release();
    }

    if(!m_vkShareHandle)
    {
      m_vkSharedTexture->Release();
      m_vkSharedTexture = nullptr;
      return;
    }

    // Create linked Vulkan image from the shared handle
    if(!linkVulkanImage(vkDevice, vkPhysDev, m_vkShareHandle, m_settings.width, m_settings.height))
    {
      m_vkSharedTexture->Release();
      m_vkSharedTexture = nullptr;
      m_vkShareHandle = nullptr;
      return;
    }

    // Register the sender
    m_created = m_dxSenderNames.CreateSender(
        (char*)m_settings.path.toStdString().c_str(), m_settings.width, m_settings.height,
        m_vkShareHandle, DXGI_FORMAT_B8G8R8A8_UNORM);
  }
#endif

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
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
        return m_vkLinkedImage != VK_NULL_HANDLE && m_vkSharedTexture != nullptr;
#endif
      default:
        return false;
    }
  }

  void render() override
  {
    auto renderer = m_renderer.lock();
    if(renderer && m_renderState)
    {
      auto rhi = m_renderState->rhi;
      QRhiCommandBuffer* cb{};
      if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        return;

      renderer->render(*cb);

#if SCORE_SPOUT_VULKAN
      // For Vulkan, do the copy BEFORE ending the frame while cb is still valid
      if(m_backend == QRhi::Vulkan && m_created)
      {
        renderVulkan(rhi, cb);
      }
#endif

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
#if SCORE_SPOUT_VULKAN
        case QRhi::Vulkan:
          // Already handled above before endOffscreenFrame
          break;
#endif
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

#if SCORE_SPOUT_VULKAN
  void renderVulkan(QRhi* rhi, QRhiCommandBuffer* cb)
  {
    if(!m_vkLinkedImage || !m_vkSharedTexture)
      return;

    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
    if(!nativeHandles || !nativeHandles->dev)
      return;

    VkDevice vkDevice = nativeHandles->dev;

    auto* inst = score::gfx::staticVulkanInstance();
    if(!inst)
      return;

    // Get source texture's VkImage
    auto nativeTex = m_texture->nativeTexture();
    VkImage srcImage = reinterpret_cast<VkImage>(nativeTex.object);
    if(!srcImage)
      return;

    // Load Vulkan functions directly
    auto vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
        inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
    auto vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(
        inst->getInstanceProcAddr("vkCmdCopyImage"));

    if(!vkCmdPipelineBarrier || !vkCmdCopyImage)
      return;

    // Get command buffer from QRhi using beginExternal/endExternal
    cb->beginExternal();

    const QRhiNativeHandles* cbNative = cb->nativeHandles();
    if(!cbNative)
    {
      cb->endExternal();
      return;
    }

    auto vkCbNative = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cbNative);
    VkCommandBuffer vkCmdBuf = vkCbNative->commandBuffer;

    if(!vkCmdBuf)
    {
      cb->endExternal();
      return;
    }

    // After rendering, the source image is in COLOR_ATTACHMENT_OPTIMAL
    // Transition source image to TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(vkCmdBuf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Transition destination (linked) image to TRANSFER_DST_OPTIMAL
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.image = m_vkLinkedImage;

    vkCmdPipelineBarrier(vkCmdBuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy the image
    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = {0, 0, 0};
    copyRegion.extent.width = m_settings.width;
    copyRegion.extent.height = m_settings.height;
    copyRegion.extent.depth = 1;

    vkCmdCopyImage(vkCmdBuf,
        srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_vkLinkedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion);

    // Transition destination to GENERAL for D3D11 access
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.image = m_vkLinkedImage;

    vkCmdPipelineBarrier(vkCmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Transition source back to COLOR_ATTACHMENT_OPTIMAL for next frame
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.image = srcImage;

    vkCmdPipelineBarrier(vkCmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    cb->endExternal();
  }
#endif

  void stopRendering() override { }

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }

  score::gfx::RenderList* renderer() const override { return m_renderer.lock().get(); }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState = std::make_shared<score::gfx::RenderState>();

    // Choose backend based on requested API
    switch(conf.graphicsApi)
    {
      case score::gfx::GraphicsApi::D3D11:
        createOutputD3D11();
        break;
      case score::gfx::GraphicsApi::D3D12:
        createOutputD3D12();
        break;
#if SCORE_SPOUT_VULKAN
      case score::gfx::GraphicsApi::Vulkan:
        createOutputVulkan();
        break;
#endif
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

    // Use BGRA for D3D/Vulkan backends, RGBA for OpenGL
    auto format = (m_backend == QRhi::D3D11 || m_backend == QRhi::D3D12 || m_backend == QRhi::Vulkan)
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

    if(conf.onReady)
      conf.onReady();
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

#if SCORE_SPOUT_VULKAN
  void createOutputVulkan()
  {
    m_backend = QRhi::Vulkan;

    // Create Vulkan instance with required extensions
    auto* vkInst = score::gfx::staticVulkanInstance();
    if(!vkInst)
    {
      qWarning() << "SpoutOutput: No Vulkan instance available";
      return;
    }

    QRhiVulkanInitParams params;
    params.inst = vkInst;

    // Enable required device extensions for external memory
    params.deviceExtensions = QRhiVulkanInitParams::preferredInstanceExtensions();
    params.deviceExtensions << VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
                            << VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME
                            << VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;

    m_renderState->rhi = QRhi::create(QRhi::Vulkan, &params, QRhi::EnableDebugMarkers, nullptr);
    m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->api = score::gfx::GraphicsApi::Vulkan;
    m_renderState->version = Gfx::Settings::shaderVersionForAPI(score::gfx::GraphicsApi::Vulkan);

    // Create a D3D11 device for creating the shared texture
    if(m_renderState->rhi)
    {
      D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
      UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

      HRESULT hr = D3D11CreateDevice(
          nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, 2,
          D3D11_SDK_VERSION, &m_vkD3D11Device, nullptr, &m_vkD3D11Context);

      if(FAILED(hr))
      {
        qWarning() << "SpoutOutput: Failed to create D3D11 device for Vulkan interop";
        m_vkD3D11Device = nullptr;
        m_vkD3D11Context = nullptr;
      }
    }
  }
#endif

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
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
      {
        m_dxSenderNames.ReleaseSenderName(m_settings.path.toStdString().c_str());
        releaseVulkanResources();
        if(m_vkSharedTexture)
        {
          m_vkSharedTexture->Release();
          m_vkSharedTexture = nullptr;
        }
        m_vkShareHandle = nullptr;
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
      }
#endif
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
#if SCORE_SPOUT_VULKAN
      case QRhi::Vulkan:
#endif
        return new Gfx::ScaledRenderer{m_rt, r.state, *this};
    }
  }

  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / m_settings.rate};
  }

#if SCORE_SPOUT_VULKAN
  bool linkVulkanImage(VkDevice vkDevice, VkPhysicalDevice vkPhysDev,
                       HANDLE dxShareHandle, unsigned int w, unsigned int h)
  {
    VkExternalMemoryHandleTypeFlags handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

    auto* inst = score::gfx::staticVulkanInstance();
    if(!inst)
      return false;
    auto funcs = inst->functions();
    if(!funcs)
      return false;
    auto dfuncs = inst->deviceFunctions(vkDevice);
    if(!dfuncs)
      return false;

    // Query support for external image format
    VkPhysicalDeviceImageFormatInfo2 formatInfo = {};
    formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    formatInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    formatInfo.type = VK_IMAGE_TYPE_2D;
    formatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    formatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo = {};
    externalFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalFormatInfo.handleType = (VkExternalMemoryHandleTypeFlagBits)handleType;
    formatInfo.pNext = &externalFormatInfo;

    VkExternalImageFormatProperties externalImageFormatProps = {};
    externalImageFormatProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 imageFormatProps2 = {};
    imageFormatProps2.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    imageFormatProps2.pNext = &externalImageFormatProps;

    auto vkGetPhysicalDeviceImageFormatProperties2Func
        = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            inst->getInstanceProcAddr("vkGetPhysicalDeviceImageFormatProperties2"));
    if(!vkGetPhysicalDeviceImageFormatProperties2Func)
      return false;

    VkResult result = vkGetPhysicalDeviceImageFormatProperties2Func(vkPhysDev, &formatInfo, &imageFormatProps2);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutOutput: KMT handle type not supported";
      return false;
    }

    VkExternalMemoryFeatureFlags externalMemoryFeatures
        = externalImageFormatProps.externalMemoryProperties.externalMemoryFeatures;
    if(!(externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
    {
      qWarning() << "SpoutOutput: Cannot import memory with KMT handle type";
      return false;
    }

    // Create the Vulkan image
    VkExternalMemoryImageCreateInfo extMemoryImageInfo = {};
    extMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extMemoryImageInfo.handleTypes = handleType;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = &extMemoryImageInfo;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    imageCreateInfo.extent = {w, h, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = dfuncs->vkCreateImage(vkDevice, &imageCreateInfo, nullptr, &m_vkLinkedImage);
    if(result != VK_SUCCESS)
    {
      qWarning() << "SpoutOutput: Could not create Vulkan image";
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
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    // Import memory
    VkImportMemoryWin32HandleInfoKHR importMemoryInfo = {};
    importMemoryInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importMemoryInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    importMemoryInfo.handle = dxShareHandle;

    bool dedicatedRequired = (externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext = &importMemoryInfo;
    dedicatedAllocInfo.image = m_vkLinkedImage;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = dedicatedRequired ? (void*)&dedicatedAllocInfo : (void*)&importMemoryInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    result = dfuncs->vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_vkLinkedMemory);
    if(result != VK_SUCCESS)
    {
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    result = dfuncs->vkBindImageMemory(vkDevice, m_vkLinkedImage, m_vkLinkedMemory, 0);
    if(result != VK_SUCCESS)
    {
      dfuncs->vkFreeMemory(vkDevice, m_vkLinkedMemory, nullptr);
      m_vkLinkedMemory = VK_NULL_HANDLE;
      dfuncs->vkDestroyImage(vkDevice, m_vkLinkedImage, nullptr);
      m_vkLinkedImage = VK_NULL_HANDLE;
      return false;
    }

    return true;
  }

  void releaseVulkanResources()
  {
    if(!m_renderState || !m_renderState->rhi)
      return;

    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(
        m_renderState->rhi->nativeHandles());
    if(!nativeHandles || !nativeHandles->dev)
    {
      m_vkLinkedImage = VK_NULL_HANDLE;
      m_vkLinkedMemory = VK_NULL_HANDLE;
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
  }
#endif

private:
  SharedOutputSettings m_settings;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
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

#if SCORE_SPOUT_VULKAN
  // Vulkan backend
  ID3D11Device* m_vkD3D11Device{};
  ID3D11DeviceContext* m_vkD3D11Context{};
  ID3D11Texture2D* m_vkSharedTexture{};
  HANDLE m_vkShareHandle{};
  VkImage m_vkLinkedImage{};
  VkDeviceMemory m_vkLinkedMemory{};
#endif

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
