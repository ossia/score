#pragma once

#if defined(__linux__)
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/DMABufImport.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Video/GpuFormats.hpp>
#include <score/gfx/Vulkan.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#if __has_include(<libavutil/hwcontext.h>)
#include <libavutil/hwcontext.h>
#endif
#if __has_include(<libavutil/hwcontext_drm.h>)
#include <libavutil/hwcontext_drm.h>
#define SCORE_HAS_DRM_HWCONTEXT_VK 1
#endif
}

#if QT_HAS_VULKAN && defined(SCORE_HAS_DRM_HWCONTEXT_VK) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>

#include <vulkan/vulkan.h>

#include <unistd.h>

// Guard against old Vulkan headers missing DRM format modifier extension
#if defined(VK_EXT_image_drm_format_modifier) && defined(VK_KHR_external_memory_fd)

namespace score::gfx
{

/**
 * @brief Zero-copy Vulkan Video decoder for the Vulkan RHI backend.
 *
 * FFmpeg decodes via Vulkan Video (AV_HWDEVICE_TYPE_VULKAN) on its own
 * VkDevice.  The decoded AVVkFrame is mapped to DRM PRIME (DMA-BUF),
 * then the per-plane file descriptors are imported into Qt's VkDevice
 * via VK_KHR_external_memory_fd + VK_EXT_image_drm_format_modifier.
 *
 * The QRhiTextures are rewrapped each frame with createFrom(), so the
 * GPU samples directly from the decoded output — no pixel copy.
 *
 * Same DMA-BUF import technique as HWVaapiVulkanDecoder, but for
 * AV_PIX_FMT_VULKAN source frames instead of AV_PIX_FMT_VAAPI.
 */
struct HWVulkanDecoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  PixelFormatInfo m_fmt;
  DMABufPlaneImporter m_importer;

  using PlaneImport = DMABufPlaneImporter::PlaneImport;

  static constexpr int NumSlots = 2;
  struct ImportSlot
  {
    PlaneImport planes[2]{};
    AVFrame* hwRef{};
  };
  ImportSlot m_slots[NumSlots]{};
  int m_slotIdx{0};

  static bool isAvailable(QRhi& rhi)
  {
    return DMABufPlaneImporter::isAvailable(rhi);
  }

  explicit HWVulkanDecoder(Video::ImageFormat& d, QRhi& rhi, PixelFormatInfo fmt)
      : decoder{d}
      , m_fmt{fmt}
  {
    m_importer.init(rhi);
  }

  ~HWVulkanDecoder() override
  {
    for(auto& slot : m_slots)
      cleanupSlot(slot);
  }

  void cleanupSlot(ImportSlot& slot)
  {
    for(auto& p : slot.planes)
      m_importer.cleanupPlane(p);
    if(slot.hwRef)
    {
      av_frame_free(&slot.hwRef);
      slot.hwRef = nullptr;
    }
  }

  // ------------------------------------------------------------------
  //  init -- create placeholder textures and NV12/P010 shaders
  // ------------------------------------------------------------------

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    if(m_fmt.is10bit())
    {
      // P010 planes: R16 (Y) + RG16 (UV)
      {
        auto tex = rhi.newTexture(QRhiTexture::R16, {w, h}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }
      {
        auto tex
            = rhi.newTexture(QRhiTexture::RG16, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }
      return score::gfx::makeShaders(
          r.state, vertexShader(),
          QString(P010Decoder::frag).arg("").arg(colorMatrix(decoder)));
    }
    else
    {
      // NV12 planes: R8 (Y) + RG8 (UV)
      {
        auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }
      {
        auto tex
            = rhi.newTexture(QRhiTexture::RG8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }

      QString frag = NV12Decoder::nv12_filter_prologue;
      frag += "    vec3 yuv = vec3(y, u, v);\n";
      frag += NV12Decoder::nv12_filter_epilogue;
      return score::gfx::makeShaders(
          r.state, vertexShader(), frag.arg("").arg(colorMatrix(decoder)));
    }
  }

  // ------------------------------------------------------------------
  //  exec -- map Vulkan frame to DMA-BUF, import into Qt's Vulkan device
  // ------------------------------------------------------------------

  void exec(RenderList& r, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if(!Video::formatIsHardwareDecoded(static_cast<AVPixelFormat>(frame.format)))
      return;

    // Advance ring-buffer slot (the one we reuse was last active 2 frames ago)
    auto& slot = m_slots[m_slotIdx];
    cleanupSlot(slot);
    m_slotIdx = (m_slotIdx + 1) % NumSlots;

    // Hold a reference to the Vulkan frame to prevent reuse
    slot.hwRef = av_frame_alloc();
    if(av_frame_ref(slot.hwRef, &frame) < 0)
    {
      av_frame_free(&slot.hwRef);
      slot.hwRef = nullptr;
      return;
    }

    // Map Vulkan frame -> DRM PRIME to obtain DMA-BUF file descriptors
    AVFrame* drmFrame = av_frame_alloc();
    drmFrame->format = AV_PIX_FMT_DRM_PRIME;

    int ret = av_hwframe_map(
        drmFrame, &frame, AV_HWFRAME_MAP_READ | AV_HWFRAME_MAP_DIRECT);
    if(ret < 0)
    {
      qDebug() << "HWVulkanDecoder: av_hwframe_map failed:" << ret;
      av_frame_free(&drmFrame);
      return;
    }

    auto* desc = reinterpret_cast<AVDRMFrameDescriptor*>(drmFrame->data[0]);
    if(!desc || desc->nb_objects < 1)
    {
      av_frame_unref(drmFrame);
      av_frame_free(&drmFrame);
      return;
    }

    // Parse plane info from the DRM descriptor.
    // NV12/P010: Y plane + UV plane.  Drivers may export them as:
    //   (a) 2 separate layers, each with 1 plane
    //   (b) 1 layer with 2 planes
    struct PlaneInfo
    {
      int obj_idx;
      ptrdiff_t offset;
      ptrdiff_t pitch;
      VkFormat format;
      int w, h;
    };
    PlaneInfo planeInfo[2]{};
    bool planesOk = false;
    const int w = decoder.width;
    const int h = decoder.height;

    const VkFormat yFmt = m_fmt.is10bit() ? VK_FORMAT_R16_UNORM : VK_FORMAT_R8_UNORM;
    const VkFormat uvFmt = m_fmt.is10bit() ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R8G8_UNORM;

    if(desc->nb_layers >= 2 && desc->layers[0].nb_planes >= 1
       && desc->layers[1].nb_planes >= 1)
    {
      // Case (a): separate layers
      auto& yP = desc->layers[0].planes[0];
      auto& uvP = desc->layers[1].planes[0];
      planeInfo[0] = {yP.object_index, yP.offset, yP.pitch, yFmt, w, h};
      planeInfo[1] = {uvP.object_index, uvP.offset, uvP.pitch, uvFmt, w / 2, h / 2};
      planesOk = true;
    }
    else if(desc->nb_layers >= 1 && desc->layers[0].nb_planes >= 2)
    {
      // Case (b): single layer, multiple planes
      auto& yP = desc->layers[0].planes[0];
      auto& uvP = desc->layers[0].planes[1];
      planeInfo[0] = {yP.object_index, yP.offset, yP.pitch, yFmt, w, h};
      planeInfo[1] = {uvP.object_index, uvP.offset, uvP.pitch, uvFmt, w / 2, h / 2};
      planesOk = true;
    }

    if(!planesOk)
    {
      qDebug() << "HWVulkanDecoder: unexpected DRM layout, layers:"
               << desc->nb_layers;
      av_frame_unref(drmFrame);
      av_frame_free(&drmFrame);
      return;
    }

    // Import each plane into a VkImage backed by the DMA-BUF memory
    for(int i = 0; i < 2; ++i)
    {
      auto& pi = planeInfo[i];
      auto& obj = desc->objects[pi.obj_idx];
      if(!m_importer.importPlane(
             slot.planes[i], obj.fd, obj.format_modifier, pi.offset, pi.pitch,
             pi.format, pi.w, pi.h))
      {
        qDebug() << "HWVulkanDecoder: importPlane failed, plane" << i;
        av_frame_unref(drmFrame);
        av_frame_free(&drmFrame);
        return;
      }
    }

    // DRM mapping can be released now -- fds were dup'd during import,
    // so Vulkan holds its own reference to the DMA-BUF buffer objects.
    av_frame_unref(drmFrame);
    av_frame_free(&drmFrame);

    // Rewrap QRhiTextures with the imported VkImages
    for(int i = 0; i < 2; ++i)
    {
      samplers[i].texture->createFrom(QRhiTexture::NativeTexture{
          quint64(slot.planes[i].image), VK_IMAGE_LAYOUT_UNDEFINED});
    }
#endif // LIBAVUTIL_VERSION_MAJOR >= 57
  }

};

} // namespace score::gfx

#endif // VK_EXT_image_drm_format_modifier && VK_KHR_external_memory_fd
#endif // QT_HAS_VULKAN && SCORE_HAS_DRM_HWCONTEXT_VK
#endif // __linux__
