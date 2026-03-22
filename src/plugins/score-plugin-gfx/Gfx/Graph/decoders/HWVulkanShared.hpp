#pragma once

#include <score/gfx/Vulkan.hpp>

#if QT_HAS_VULKAN

extern "C" {
#if __has_include(<libavutil/hwcontext_vulkan.h>)
#include <libavutil/hwcontext_vulkan.h>
#define SCORE_HAS_VULKAN_HWCONTEXT_SHARED 1
#endif
}

#if defined(SCORE_HAS_VULKAN_HWCONTEXT_SHARED) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV420P10.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUV444.hpp>
#include <Gfx/Graph/decoders/YUV444P10.hpp>
#include <Gfx/Graph/decoders/YUVA444.hpp>
#include <Video/GpuFormats.hpp>

// Qt private header for QVkTexture internals
#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>
#include <vulkan/vulkan.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace score::gfx
{

/**
 * @brief True zero-copy Vulkan Video decoder using shared VkDevice.
 *
 * FFmpeg's Vulkan Video decoder produces multiplane VkImages. We create
 * per-plane VkImageViews (VK_IMAGE_ASPECT_PLANE_x_BIT) and patch them
 * directly into QVkTexture internals. A custom pipeline barrier with the
 * correct plane aspects ensures memory visibility.
 *
 * No copies at all — decode output is sampled directly by the fragment shader.
 */
struct HWVulkanSharedDecoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  PixelFormatInfo m_fmt;
  int m_numPlanes{0};

  // Vulkan handles
  VkDevice m_dev{VK_NULL_HANDLE};
  VkPhysicalDevice m_physDev{VK_NULL_HANDLE};
  QVulkanFunctions* m_funcs{};
  QVulkanDeviceFunctions* m_dfuncs{};
  PFN_vkWaitSemaphores m_vkWaitSemaphores{};
  uint32_t m_gfxQueueFamilyIdx{0};
  VkQueue m_gfxQueue{VK_NULL_HANDLE};

  // Command infrastructure for custom multiplane barriers
  VkCommandPool m_cmdPool{VK_NULL_HANDLE};
  VkCommandBuffer m_cmdBuf{VK_NULL_HANDLE};
  VkFence m_fence{VK_NULL_HANDLE};
  bool m_cmdReady{false};

  // Ring buffer for frame lifetime + deferred view destruction
  static constexpr int NumSlots = 3;
  struct FrameSlot
  {
    AVFrame* frameRef{};
    VkImageView planeViews[4]{};
    int numViews{0};
  };
  FrameSlot m_slots[NumSlots]{};
  int m_slotIdx{0};

  // ------------------------------------------------------------------

  static bool isAvailable(QRhi& rhi)
  {
    if(rhi.backend() != QRhi::Vulkan)
      return false;
    auto* nh
        = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nh || !nh->dev || !nh->physDev || !nh->inst)
      return false;
    return nh->inst->getInstanceProcAddr("vkWaitSemaphores") != nullptr;
  }

  explicit HWVulkanSharedDecoder(
      Video::ImageFormat& d, QRhi& rhi, PixelFormatInfo fmt)
      : decoder{d}
      , m_fmt{fmt}
  {
    auto* nh
        = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    m_dev = nh->dev;
    m_physDev = nh->physDev;
    m_funcs = nh->inst->functions();
    m_dfuncs = nh->inst->deviceFunctions(m_dev);
    m_gfxQueueFamilyIdx = nh->gfxQueueFamilyIdx;
    m_vkWaitSemaphores = reinterpret_cast<PFN_vkWaitSemaphores>(
        nh->inst->getInstanceProcAddr("vkWaitSemaphores"));
    m_dfuncs->vkGetDeviceQueue(
        m_dev, m_gfxQueueFamilyIdx, 0, &m_gfxQueue);
  }

  ~HWVulkanSharedDecoder() override
  {
    // Wait for in-flight rendering to complete before destroying
    // VkImageViews and freeing AVFrames (which release the VkImages).
    // Without this, the last render pass command buffer may still
    // reference these resources.
    // Only wait on the graphics queue — lighter than vkDeviceWaitIdle.
    if(m_gfxQueue != VK_NULL_HANDLE)
      m_dfuncs->vkQueueWaitIdle(m_gfxQueue);

    for(auto& slot : m_slots)
      cleanupSlot(slot);
    if(m_fence != VK_NULL_HANDLE)
      m_dfuncs->vkDestroyFence(m_dev, m_fence, nullptr);
    if(m_cmdPool != VK_NULL_HANDLE)
      m_dfuncs->vkDestroyCommandPool(m_dev, m_cmdPool, nullptr);
  }

  void cleanupSlot(FrameSlot& slot)
  {
    for(int i = 0; i < slot.numViews; i++)
    {
      if(slot.planeViews[i] != VK_NULL_HANDLE)
      {
        m_dfuncs->vkDestroyImageView(m_dev, slot.planeViews[i], nullptr);
        slot.planeViews[i] = VK_NULL_HANDLE;
      }
    }
    slot.numViews = 0;
    if(slot.frameRef)
    {
      av_frame_free(&slot.frameRef);
      slot.frameRef = nullptr;
    }
  }

  bool setupCommandInfra()
  {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_gfxQueueFamilyIdx;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if(m_dfuncs->vkCreateCommandPool(m_dev, &poolInfo, nullptr, &m_cmdPool)
       != VK_SUCCESS)
      return false;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if(m_dfuncs->vkAllocateCommandBuffers(m_dev, &allocInfo, &m_cmdBuf)
       != VK_SUCCESS)
      return false;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if(m_dfuncs->vkCreateFence(m_dev, &fenceInfo, nullptr, &m_fence)
       != VK_SUCCESS)
      return false;

    m_cmdReady = true;
    return true;
  }

  // ------------------------------------------------------------------
  //  init -- create placeholder textures and shaders
  // ------------------------------------------------------------------

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;
    const bool is10 = m_fmt.is10bit();
    auto texFmt = is10 ? QRhiTexture::R16 : QRhiTexture::R8;
    int chromaW = AV_CEIL_RSHIFT(w, m_fmt.log2ChromaW);
    int chromaH = AV_CEIL_RSHIFT(h, m_fmt.log2ChromaH);

    // Semi-planar (2 planes: Y + UV) vs planar (3 planes: Y + U + V)
    // Vulkan Video always outputs multiplane images; the plane count
    // depends on the sw_format negotiated with FFmpeg.
    // Semi-planar: NV12, P010, P210, P410, etc.
    // Planar: YUV420P, YUV422P, YUV444P, etc.
    m_numPlanes = m_fmt.numPlanes;

    if(m_numPlanes == 2)
    {
      auto uvFmt = is10 ? QRhiTexture::RG16 : QRhiTexture::RG8;
      createTex(rhi, texFmt, w, h);
      createTex(rhi, uvFmt, chromaW, chromaH);

      if(is10)
        return score::gfx::makeShaders(
            r.state, vertexShader(),
            QString(P010Decoder::frag).arg("").arg(colorMatrix(decoder)));
      else
      {
        QString frag = NV12Decoder::nv12_filter_prologue;
        frag += "    vec3 yuv = vec3(y, u, v);\n";
        frag += NV12Decoder::nv12_filter_epilogue;
        return score::gfx::makeShaders(
            r.state, vertexShader(),
            frag.arg("").arg(colorMatrix(decoder)));
      }
    }
    else if(m_fmt.hasAlpha)
    {
      // 4 planes: Y + U + V + A (YUVA444, YUVA444P10, YUVA444P12, etc.)
      createTex(rhi, texFmt, w, h);
      createTex(rhi, texFmt, chromaW, chromaH);
      createTex(rhi, texFmt, chromaW, chromaH);
      createTex(rhi, texFmt, w, h); // Alpha at full resolution

      if(!is10)
      {
        return score::gfx::makeShaders(
            r.state, vertexShader(),
            QString(YUVA444Decoder::frag).arg("").arg(colorMatrix(decoder)));
      }
      else
      {
        // R16_UNORM samples as raw_value/65535. Scale to actual bit range.
        // 10-bit: 65535/1023 ≈ 64. 12-bit: 65535/4095 = 16. 16-bit: 1.
        double scale = 65535.0 / ((1 << m_fmt.bitDepth) - 1);
        QString frag = QString(R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D u_tex;
layout(binding=5) uniform sampler2D v_tex;
layout(binding=6) uniform sampler2D a_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main()
{
  float sc = float(%3);
  float y = sc * texture(y_tex, v_texcoord).r;
  float u = sc * texture(u_tex, v_texcoord).r;
  float v = sc * texture(v_tex, v_texcoord).r;
  float a = sc * texture(a_tex, v_texcoord).r;

  vec4 rgb = processTexture(vec4(y,u,v, 1.));
  fragColor = vec4(rgb.rgb, a);
}
)_").arg("").arg(colorMatrix(decoder)).arg(scale, 0, 'f', 6);

        return score::gfx::makeShaders(r.state, vertexShader(), frag);
      }
    }
    else
    {
      // 3 planes: Y + U + V (YUV420P, YUV422P, YUV444P, etc.)
      createTex(rhi, texFmt, w, h);
      createTex(rhi, texFmt, chromaW, chromaH);
      createTex(rhi, texFmt, chromaW, chromaH);

      if(!is10)
      {
        const char* fragSrc = YUV420Decoder::frag;
        if(m_fmt.log2ChromaW == 1 && m_fmt.log2ChromaH == 0)
          fragSrc = YUV422Decoder::frag;
        else if(m_fmt.log2ChromaW == 0 && m_fmt.log2ChromaH == 0)
          fragSrc = YUV444Decoder::frag;
        return score::gfx::makeShaders(
            r.state, vertexShader(),
            QString(fragSrc).arg("").arg(colorMatrix(decoder)));
      }
      else
      {
        // R16_UNORM: scale from actual bit depth to normalized range
        double scale = 65535.0 / ((1 << m_fmt.bitDepth) - 1);
        QString frag = QString(R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D u_tex;
layout(binding=5) uniform sampler2D v_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main()
{
  float sc = float(%3);
  float y = sc * texture(y_tex, v_texcoord).r;
  float u = sc * texture(u_tex, v_texcoord).r;
  float v = sc * texture(v_tex, v_texcoord).r;

  fragColor = processTexture(vec4(y,u,v, 1.));
}
)_").arg("").arg(colorMatrix(decoder)).arg(scale, 0, 'f', 6);

        return score::gfx::makeShaders(r.state, vertexShader(), frag);
      }
    }
  }

  // ------------------------------------------------------------------
  //  exec -- per-plane VkImageViews on multiplane image (true zero-copy)
  // ------------------------------------------------------------------

  void exec(RenderList& r, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if(!Video::formatIsHardwareDecoded(
           static_cast<AVPixelFormat>(frame.format)))
      return;

    auto* vkf = reinterpret_cast<AVVkFrame*>(frame.data[0]);
    if(!vkf || vkf->img[0] == VK_NULL_HANDLE)
      return;

    if(!m_cmdReady && !setupCommandInfra())
      return;

    // Wait on FFmpeg's timeline semaphores (host-side)
    if(m_vkWaitSemaphores && vkf->sem[0] != VK_NULL_HANDLE)
    {
      int numSems = 0;
      for(int i = 0; i < 4; i++)
        if(vkf->sem[i] != VK_NULL_HANDLE)
          numSems = i + 1;
      if(numSems > 0)
      {
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = static_cast<uint32_t>(numSems);
        waitInfo.pSemaphores = vkf->sem;
        waitInfo.pValues = vkf->sem_value;
        m_vkWaitSemaphores(m_dev, &waitInfo, UINT64_MAX);
      }
    }

    // Ring buffer management
    auto& slot = m_slots[m_slotIdx];
    cleanupSlot(slot);
    m_slotIdx = (m_slotIdx + 1) % NumSlots;

    slot.frameRef = av_frame_alloc();
    if(av_frame_ref(slot.frameRef, &frame) < 0)
    {
      av_frame_free(&slot.frameRef);
      slot.frameRef = nullptr;
      return;
    }

    // Count separate VkImages
    int numSrcImages = 0;
    for(int i = 0; i < m_numPlanes; i++)
      if(vkf->img[i] != VK_NULL_HANDLE)
        numSrcImages++;

    const bool isMultiplane = (numSrcImages == 1 && m_numPlanes > 1);

    if(isMultiplane)
    {
      // --- Submit a custom barrier for the multiplane image ---
      // QRhi would use VK_IMAGE_ASPECT_COLOR_BIT which is invalid for
      // multiplane. We submit our own barrier with the correct plane aspects,
      // transitioning to SHADER_READ_ONLY_OPTIMAL. Then we tell QRhi the
      // image is already in that layout so it doesn't insert its own barrier.

      m_dfuncs->vkResetCommandBuffer(m_cmdBuf, 0);

      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      m_dfuncs->vkBeginCommandBuffer(m_cmdBuf, &beginInfo);

      // One barrier per plane with the correct aspect mask
      VkImageMemoryBarrier barriers[4]{};
      static const VkImageAspectFlagBits planeAspects[] = {
          VK_IMAGE_ASPECT_PLANE_0_BIT,
          VK_IMAGE_ASPECT_PLANE_1_BIT,
          VK_IMAGE_ASPECT_PLANE_2_BIT,
      };

      for(int i = 0; i < m_numPlanes && i < 3; i++)
      {
        barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[i].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[i].oldLayout = vkf->layout[0];
        barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].image = vkf->img[0];
        barriers[i].subresourceRange.aspectMask = planeAspects[i];
        barriers[i].subresourceRange.baseMipLevel = 0;
        barriers[i].subresourceRange.levelCount = 1;
        barriers[i].subresourceRange.baseArrayLayer = 0;
        barriers[i].subresourceRange.layerCount = 1;
      }

      m_dfuncs->vkCmdPipelineBarrier(
          m_cmdBuf,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          0, 0, nullptr, 0, nullptr,
          static_cast<uint32_t>(m_numPlanes), barriers);

      m_dfuncs->vkEndCommandBuffer(m_cmdBuf);

      m_dfuncs->vkResetFences(m_dev, 1, &m_fence);
      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &m_cmdBuf;
      m_dfuncs->vkQueueSubmit(m_gfxQueue, 1, &submitInfo, m_fence);
      m_dfuncs->vkWaitForFences(m_dev, 1, &m_fence, VK_TRUE, UINT64_MAX);

      // --- Create per-plane VkImageViews and patch QVkTexture ---
      for(int i = 0; i < m_numPlanes && i < (int)samplers.size(); i++)
      {
        auto* vkTex = static_cast<QVkTexture*>(samplers[i].texture);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vkf->img[0];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vkTex->vkformat;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange.aspectMask = planeAspects[i];
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView planeView = VK_NULL_HANDLE;
        if(m_dfuncs->vkCreateImageView(
               m_dev, &viewInfo, nullptr, &planeView)
           != VK_SUCCESS)
          return;

        slot.planeViews[i] = planeView;
        slot.numViews = i + 1;

        // Destroy QRhi's current view
        if(vkTex->imageView != VK_NULL_HANDLE)
          m_dfuncs->vkDestroyImageView(m_dev, vkTex->imageView, nullptr);

        vkTex->image = vkf->img[0];
        vkTex->imageView = planeView;
        vkTex->owns = false;
        // Already transitioned to SHADER_READ_ONLY by our barrier
        vkTex->usageState.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkTex->usageState.access = VK_ACCESS_SHADER_READ_BIT;
        vkTex->usageState.stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        vkTex->generation++;
      }
    }
    else
    {
      // Separate per-plane VkImages: direct createFrom() wrapping
      for(int i = 0; i < m_numPlanes && i < (int)samplers.size(); i++)
      {
        if(vkf->img[i] != VK_NULL_HANDLE)
        {
          samplers[i].texture->createFrom(QRhiTexture::NativeTexture{
              quint64(vkf->img[i]), int(vkf->layout[i])});
        }
      }
    }
#endif
  }

private:
  void createTex(QRhi& rhi, QRhiTexture::Format fmt, int w, int h)
  {
    auto tex = rhi.newTexture(fmt, {w, h}, 1, QRhiTexture::Flag{});
    tex->create();
    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }
};

} // namespace score::gfx

#endif // SCORE_HAS_VULKAN_HWCONTEXT_SHARED && QT_VERSION >= 6.6
#endif // QT_HAS_VULKAN
